// One-click Overlord updater. Built by CI with the .NET Framework csc.exe that
// ships with Windows, so it uses C# 5 syntax only. The client overlay zip is
// embedded as the "package.zip" resource.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Reflection;

static class Installer
{
	const string DefaultGameDirectory = @"C:\Program Files (x86)\Call of Duty Modern Warfare 2 Campaign Remastered";
	const string BackupFolder = "overlord-backups";
	const string AddedList = "added-files.txt";
	static readonly char Separator = Path.DirectorySeparatorChar;
	static readonly string[] GameProcesses = { "overlord", "overlord-debug", "MW2CR", "h2_sp64_bnet_ship" };

	static int Main()
	{
		Console.Title = "Overlord VR updater";
		try
		{
			Console.WriteLine("Overlord VR updater");
			Console.WriteLine("===================");
			Console.WriteLine();
			var game = FindGameDirectory();
			if (game == null) return Finish(1);
			Console.WriteLine("Game folder: " + game);
			var installed = File.Exists(Path.Combine(game, "version.json"))
				? VersionName(File.ReadAllText(Path.Combine(game, "version.json"))) : null;
			Console.WriteLine("Installed version: " + (installed ?? "unknown"));
			Console.WriteLine("This installer:    " + (PackagedVersion() ?? "unknown"));
			Console.WriteLine();
			Console.WriteLine("  1) Install / update the VR mod (recommended)");
			Console.WriteLine("  2) Undo the last update");
			Console.WriteLine("  3) Exit");
			Console.Write("Choose 1, 2 or 3 and press Enter [1]: ");
			var choice = (Console.ReadLine() ?? "").Trim();
			if (choice == "3") return Finish(0);
			if (!CloseGame()) return Finish(1);
			return Finish(choice == "2" ? Undo(game) : Install(game));
		}
		catch (Exception error)
		{
			Console.WriteLine();
			Console.WriteLine("Something went wrong: " + error.Message);
			Console.WriteLine("Nothing after this point was changed. Your backups are in the overlord-backups folder.");
			return Finish(1);
		}
	}

	static int Finish(int code)
	{
		Console.WriteLine();
		Console.Write("Press Enter to close.");
		Console.ReadLine();
		return code;
	}

	// version.json is plain {"name": "..."}; avoid a JSON dependency for one field.
	static string VersionName(string json)
	{
		var match = System.Text.RegularExpressions.Regex.Match(json ?? "", "\"name\"\\s*:\\s*\"([^\"]{1,64})\"");
		return match.Success ? match.Groups[1].Value : null;
	}

	static string PackagedVersion()
	{
		var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream("package.zip");
		if (stream == null) return null;
		using (stream)
		using (var archive = new ZipArchive(stream, ZipArchiveMode.Read))
		{
			var entry = archive.GetEntry("version.json");
			if (entry == null) return null;
			using (var reader = new StreamReader(entry.Open()))
				return VersionName(reader.ReadToEnd());
		}
	}

	static bool IsGameDirectory(string directory)
	{
		return !string.IsNullOrEmpty(directory) && Directory.Exists(directory) &&
			(File.Exists(Path.Combine(directory, "MW2CR.exe")) || File.Exists(Path.Combine(directory, "overlord.exe")));
	}

	// Prefer the folder an existing desktop shortcut launches, so the copy the
	// player actually uses is the one updated.
	static string FindGameDirectory()
	{
		var candidates = new List<string>();
		foreach (var desktop in new[] {
			Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory),
			Environment.GetFolderPath(Environment.SpecialFolder.CommonDesktopDirectory) })
		{
			if (string.IsNullOrEmpty(desktop) || !Directory.Exists(desktop)) continue;
			foreach (var link in Directory.GetFiles(desktop, "*.lnk"))
			{
				var target = ShortcutTarget(link);
				if (string.IsNullOrEmpty(target)) continue;
				var name = Path.GetFileNameWithoutExtension(target);
				var title = Path.GetFileNameWithoutExtension(link);
				if (name.StartsWith("overlord", StringComparison.OrdinalIgnoreCase) ||
					title.IndexOf("MW2 VR", StringComparison.OrdinalIgnoreCase) >= 0)
					candidates.Add(Path.GetDirectoryName(target));
			}
		}
		candidates.Add(DefaultGameDirectory);
		foreach (var candidate in candidates.Distinct(StringComparer.OrdinalIgnoreCase))
			if (IsGameDirectory(candidate)) return candidate;

		Console.WriteLine("Could not find the game folder automatically.");
		while (true)
		{
			Console.Write("Paste the game folder path (the folder with MW2CR.exe), or leave empty to cancel: ");
			var typed = (Console.ReadLine() ?? "").Trim().Trim('"');
			if (typed.Length == 0) return null;
			if (IsGameDirectory(typed)) return typed;
			Console.WriteLine("That folder does not contain MW2CR.exe or overlord.exe.");
		}
	}

	static string ShortcutTarget(string link)
	{
		try
		{
			var shellType = Type.GetTypeFromProgID("WScript.Shell");
			if (shellType == null) return null;
			var shell = Activator.CreateInstance(shellType);
			var shortcut = shellType.InvokeMember("CreateShortcut", BindingFlags.InvokeMethod, null, shell, new object[] { link });
			return (string)shortcut.GetType().InvokeMember("TargetPath", BindingFlags.GetProperty, null, shortcut, null);
		}
		catch
		{
			return null;
		}
	}

	static bool CloseGame()
	{
		while (true)
		{
			var running = GameProcesses.SelectMany(name => Process.GetProcessesByName(name)).ToList();
			if (running.Count == 0) return true;
			Console.WriteLine();
			Console.WriteLine("The game or the Overlord launcher is still running.");
			Console.Write("Close it, then press Enter to continue (or type Q to cancel): ");
			if ((Console.ReadLine() ?? "").Trim().Equals("q", StringComparison.OrdinalIgnoreCase)) return false;
		}
	}

	static int Install(string game)
	{
		var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream("package.zip");
		if (stream == null)
		{
			Console.WriteLine("This installer is missing its mod files. Download it again.");
			return 1;
		}
		using (stream)
		using (var archive = new ZipArchive(stream, ZipArchiveMode.Read))
		{
			var entries = archive.Entries.Where(e => e.Name.Length > 0).ToList();
			var root = Path.GetFullPath(game).TrimEnd(Separator) + Separator;
			var backup = Path.Combine(game, BackupFolder, DateTime.Now.ToString("yyyy-MM-dd_HH-mm-ss"));
			Directory.CreateDirectory(backup);
			var added = new List<string>();

			Console.WriteLine();
			Console.WriteLine("Backing up the files that will be replaced...");
			foreach (var entry in entries)
			{
				var target = SafePath(root, entry.FullName);
				if (File.Exists(target))
				{
					var copy = Path.Combine(backup, entry.FullName.Replace('/', Separator));
					Directory.CreateDirectory(Path.GetDirectoryName(copy));
					File.Copy(target, copy, true);
				}
				else added.Add(entry.FullName.Replace('/', Separator));
			}
			File.WriteAllLines(Path.Combine(backup, AddedList), added.ToArray());
			Console.WriteLine("Backup saved to " + backup);

			Console.WriteLine("Installing " + entries.Count + " files...");
			foreach (var entry in entries)
			{
				var target = SafePath(root, entry.FullName);
				Directory.CreateDirectory(Path.GetDirectoryName(target));
				entry.ExtractToFile(target, true);
			}
		}
		Console.WriteLine();
		Console.WriteLine("Done! Start SteamVR, then launch the game with your MW2 VR shortcut.");
		Console.WriteLine("In game, stand in the middle of your play space and hold left X to recenter.");
		Console.WriteLine("If anything is wrong, run this installer again and choose 2 to undo.");
		return 0;
	}

	static int Undo(string game)
	{
		var backups = Path.Combine(game, BackupFolder);
		var latest = Directory.Exists(backups)
			? Directory.GetDirectories(backups).Where(d => !d.EndsWith("-restored", StringComparison.OrdinalIgnoreCase))
				.OrderByDescending(d => d, StringComparer.Ordinal).FirstOrDefault()
			: null;
		if (latest == null)
		{
			Console.WriteLine("No backup found, so there is nothing to undo.");
			return 1;
		}
		var root = Path.GetFullPath(game).TrimEnd(Separator) + Separator;
		Console.WriteLine();
		Console.WriteLine("Restoring the backup from " + Path.GetFileName(latest) + "...");
		var addedFile = Path.Combine(latest, AddedList);
		if (File.Exists(addedFile))
			foreach (var relative in File.ReadAllLines(addedFile))
			{
				if (relative.Trim().Length == 0) continue;
				var target = SafePath(root, relative);
				if (File.Exists(target)) File.Delete(target);
			}
		foreach (var saved in Directory.GetFiles(latest, "*", SearchOption.AllDirectories))
		{
			var relative = saved.Substring(latest.Length).TrimStart(Separator);
			if (relative.Equals(AddedList, StringComparison.OrdinalIgnoreCase)) continue;
			var target = SafePath(root, relative);
			Directory.CreateDirectory(Path.GetDirectoryName(target));
			File.Copy(saved, target, true);
		}
		Directory.Move(latest, latest + "-restored");
		Console.WriteLine("Your previous version is back.");
		return 0;
	}

	// Never write outside the game folder, whatever the archive contains.
	static string SafePath(string root, string relative)
	{
		var full = Path.GetFullPath(Path.Combine(root, relative.Replace('/', Separator)));
		if (!full.StartsWith(root, StringComparison.OrdinalIgnoreCase))
			throw new InvalidDataException("Refusing to write outside the game folder: " + relative);
		return full;
	}
}
