using System.IO;
using System.Collections.Generic;
using System.Diagnostics;

namespace IVOESpaces.Core.Services;

/// <summary>Smart program organizer - categorizes applications by type</summary>
public class SmartOrganizer
{
    public enum ProgramCategory
    {
        TextAndDocs,
        Communication,
        Development,
        Games,
        GameLaunchers,
        Multimedia,
        Utilities,
        Office,
        Uncategorized
    }

    private static readonly Dictionary<string, ProgramCategory> CategoryMap = new(StringComparer.OrdinalIgnoreCase)
    {
        // Text & Documents
        { "notepad", ProgramCategory.TextAndDocs },
        { "word", ProgramCategory.TextAndDocs },
        { "excel", ProgramCategory.TextAndDocs },
        { "powerpoint", ProgramCategory.TextAndDocs },
        { "adobe", ProgramCategory.TextAndDocs },
        { "acrobat", ProgramCategory.TextAndDocs },
        { "libreoffice", ProgramCategory.TextAndDocs },
        
        // Communication
        { "outlook", ProgramCategory.Communication },
        { "mail", ProgramCategory.Communication },
        { "discord", ProgramCategory.Communication },
        { "slack", ProgramCategory.Communication },
        { "teams", ProgramCategory.Communication },
        { "telegram", ProgramCategory.Communication },
        { "whatsapp", ProgramCategory.Communication },
        { "zoom", ProgramCategory.Communication },
        { "skype", ProgramCategory.Communication },
        
        // Development
        { "visual studio", ProgramCategory.Development },
        { "vscode", ProgramCategory.Development },
        { "code", ProgramCategory.Development },
        { "github", ProgramCategory.Development },
        { "git", ProgramCategory.Development },
        { "terminal", ProgramCategory.Development },
        { "cmd", ProgramCategory.Development },
        { "powershell", ProgramCategory.Development },
        { "python", ProgramCategory.Development },
        { "node", ProgramCategory.Development },
        { "intellij", ProgramCategory.Development },
        
        // Games & Game Launchers
        { "steam", ProgramCategory.GameLaunchers },
        { "epic", ProgramCategory.GameLaunchers },
        { "uplay", ProgramCategory.GameLaunchers },
        { "origin", ProgramCategory.GameLaunchers },
        { "battlenet", ProgramCategory.GameLaunchers },
        { "launcher", ProgramCategory.GameLaunchers },
        
        // Multimedia
        { "spotify", ProgramCategory.Multimedia },
        { "vlc", ProgramCategory.Multimedia },
        { "media", ProgramCategory.Multimedia },
        { "photoshop", ProgramCategory.Multimedia },
        { "lightroom", ProgramCategory.Multimedia },
        { "premiere", ProgramCategory.Multimedia },
        { "after effects", ProgramCategory.Multimedia },
        { "audacity", ProgramCategory.Multimedia },
        
        // Office
        { "calendar", ProgramCategory.Office },
        { "todo", ProgramCategory.Office },
        { "notion", ProgramCategory.Office },
        { "onenote", ProgramCategory.Office }
    };

    public static ProgramCategory CategorizeProgram(string programName)
    {
        var nameLower = programName.ToLower();
        
        foreach (var entry in CategoryMap)
        {
            if (nameLower.Contains(entry.Key))
                return entry.Value;
        }

        // Check if it looks like a game by common patterns
        if (nameLower.Contains("game") || nameLower.Contains("play"))
            return ProgramCategory.Games;

        return ProgramCategory.Uncategorized;
    }

    public static List<(string Name, ProgramCategory Category)> GetInstalledPrograms()
    {
        var programs = new List<(string, ProgramCategory)>();
        var progFiles = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles);
        var progFilesX86 = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86);
        var appData = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "Microsoft", "Windows", "Start Menu", "Programs");

        try
        {
            // Scan program shortcuts
            if (Directory.Exists(appData))
            {
                var shortcuts = Directory.GetFiles(appData, "*.lnk", SearchOption.AllDirectories);
                foreach (var shortcut in shortcuts.Take(100))
                {
                    try
                    {
                        var name = Path.GetFileNameWithoutExtension(shortcut);
                        var category = CategorizeProgram(name);
                        programs.Add((name, category));
                    }
                    catch (Exception ex)
                    {
                        Serilog.Log.Warning(ex, "Failed to categorize shortcut");
                    }
                }
            }
        }
        catch (Exception ex)
        {
            Serilog.Log.Error(ex, "Failed to scan installed programs");
        }

        return programs;
    }

    public static Dictionary<ProgramCategory, List<string>> GroupByCategory(List<string> programNames)
    {
        var grouped = new Dictionary<ProgramCategory, List<string>>();
        
        foreach (ProgramCategory cat in Enum.GetValues(typeof(ProgramCategory)))
        {
            grouped[cat] = new List<string>();
        }

        foreach (var program in programNames)
        {
            var category = CategorizeProgram(program);
            grouped[category].Add(program);
        }

        return grouped;
    }

    public static List<string> GetMostUsedPrograms(int count = 10)
    {
        var programs = new List<string>();
        
        try
        {
            // Try to read from Windows registry (most used programs)
            var key = Microsoft.Win32.Registry.CurrentUser.OpenSubKey(
                @"Software\Microsoft\Windows\CurrentVersion\Explorer\ComDlg32\LastVisitedMRU");
            
            if (key != null)
            {
                var values = key.GetValueNames();
                programs.AddRange(values.Take(count).Select(v => key.GetValue(v)?.ToString() ?? ""));
                programs = programs.Where(p => !string.IsNullOrEmpty(p)).ToList();
            }
        }
        catch (Exception ex)
        {
            Serilog.Log.Warning(ex, "Failed to read most-used programs from registry");
        }
        
        return programs.Take(count).ToList();
    }
}
