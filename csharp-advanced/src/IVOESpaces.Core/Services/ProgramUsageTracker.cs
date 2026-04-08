using System.IO;
using System.Text.Json;

namespace IVOESpaces.Core.Services;

/// <summary>Tracks program usage statistics</summary>
public class ProgramUsageTracker
{
    private const string UsageDataFile = "program_usage.json";
    private readonly Dictionary<string, int> _usageStats = new();
    private readonly string _dataPath;

    public ProgramUsageTracker()
    {
        _dataPath = Path.Combine(AppPaths.DataRoot, UsageDataFile);
        LoadUsageData();
    }

    public void RecordUsage(string programName)
    {
        if (string.IsNullOrEmpty(programName)) return;

        if (_usageStats.ContainsKey(programName))
            _usageStats[programName]++;
        else
            _usageStats[programName] = 1;

        SaveUsageData();
    }

    public List<(string Program, int Count)> GetMostUsed(int count = 10)
    {
        return _usageStats
            .OrderByDescending(x => x.Value)
            .Take(count)
            .Select(x => (x.Key, x.Value))
            .ToList();
    }

    private void LoadUsageData()
    {
        try
        {
            if (!File.Exists(_dataPath))
                return;

            var json = File.ReadAllText(_dataPath);
            var data = JsonSerializer.Deserialize<Dictionary<string, int>>(json);
            if (data != null)
            {
                _usageStats.Clear();
                foreach (var kvp in data)
                    _usageStats[kvp.Key] = kvp.Value;
            }
        }
        catch { }
    }

    private void SaveUsageData()
    {
        try
        {
            var json = JsonSerializer.Serialize(_usageStats, new JsonSerializerOptions { WriteIndented = true });
            File.WriteAllText(_dataPath, json);
        }
        catch { }
    }
}
