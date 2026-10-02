using System.Diagnostics;
using System.IO.Compression;
using System.Reflection;
using System.Security.Cryptography;
using System.Security.Principal;
using System.Text.Json;

namespace PenumbraVR.Setup;

internal static class Program
{
    [STAThread]
    private static int Main(string[] args)
    {
        ApplicationConfiguration.Initialize();
        bool smoke = args.Contains("--smoke-test");
        string language = Value(args, "--language") ?? "en";
        if (language is not ("en" or "es")) return 2;
        string? export = Value(args, "--extract-to");
        try
        {
            // Normal installation needs write access to Steam's Program Files roots.
            // Read-only packaging diagnostics never request elevation.
            if (!smoke && export is null && !IsAdministrator())
            {
                var elevated = new ProcessStartInfo(Environment.ProcessPath!)
                { UseShellExecute = true, Verb = "runas" };
                foreach (string arg in args) elevated.ArgumentList.Add(arg);
                using var child = Process.Start(elevated);
                return child is null ? 1 : 0;
            }
            if (export is not null)
            {
                if (Directory.Exists(export) || File.Exists(export))
                    throw new IOException("Use a new extraction directory.");
                Extract(Path.GetFullPath(export));
                return 0;
            }
            string root = Path.Combine(Path.GetTempPath(), "PenumbraVR-Setup-" + Guid.NewGuid().ToString("N"));
            object? report = null;
            int result = 0;
            try
            {
                bool preparationShown = false;
                int preparationTicks = 0;
                if (!smoke || args.Contains("--show-preparation"))
                    (result, preparationShown, preparationTicks) = RunPrepared(root, language, smoke);
                else
                    Extract(root);
                if (smoke)
                {
                    report = new { language, spanishTranslations = language == "es",
                        payloadEmbedded = true, bannerEmbedded = true,
                        payloadExtracted = File.Exists(Path.Combine(root, "tools", "Install-PenumbraFrameworkGui.ps1")),
                        tempCleaned = true, preparationShown, preparationTicks };
                }
            }
            finally
            {
                if (Directory.Exists(root)) Directory.Delete(root, true);
                File.Delete(root + ".ready");
            }
            if (report is not null) Console.WriteLine(JsonSerializer.Serialize(report));
            return result;
        }
        catch (Exception ex)
        {
            if (smoke || export is not null) Console.Error.WriteLine(ex.Message);
            else MessageBox.Show(ex.Message, "Penumbra VR Setup", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return 1;
        }
    }

    private static (int ExitCode, bool Shown, int PumpTicks) RunPrepared(
        string root, string language, bool smoke)
    {
        using var window = new Form
        {
            Text = "Penumbra VR Setup",
            ClientSize = new System.Drawing.Size(480, 145),
            StartPosition = FormStartPosition.CenterScreen,
            FormBorderStyle = FormBorderStyle.FixedDialog,
            ControlBox = false
        };
        if (smoke)
        {
            window.StartPosition = FormStartPosition.Manual;
            window.Location = new System.Drawing.Point(-32000, -32000);
            window.ShowInTaskbar = false;
        }
        window.Controls.Add(new Label
        {
            Text = language == "es"
                ? "Preparando la instalación…\nExtrayendo y verificando los archivos."
                : "Preparing installation…\nExtracting and verifying files.",
            Location = new System.Drawing.Point(20, 20),
            Size = new System.Drawing.Size(440, 60)
        });
        window.Controls.Add(new ProgressBar
        {
            Style = ProgressBarStyle.Marquee, MarqueeAnimationSpeed = 25,
            Location = new System.Drawing.Point(20, 100),
            Size = new System.Drawing.Size(440, 20)
        });
        // Keep transient UI state outside the checksum-covered payload tree.
        string ready = root + ".ready";
        Task<int>? work = null;
        bool shown = false, finished = false;
        int ticks = 0;
        using var poll = new System.Windows.Forms.Timer { Interval = 50 };
        window.Shown += (_, _) =>
        {
            shown = true;
            work = Task.Run(() =>
            {
                Extract(root);
                return smoke ? 0 : RunGui(root, language, ready);
            });
            poll.Start();
        };
        window.FormClosing += (_, e) => e.Cancel = !finished;
        poll.Tick += (_, _) =>
        {
            ++ticks;
            if (work is null || (!work.IsCompleted && !File.Exists(ready))) return;
            finished = true;
            poll.Stop();
            window.Close();
        };
        // The preparation window paints before extraction starts and stays up
        // until the PowerShell form signals that it is visible. Payload cleanup
        // remains owned by Main, after the child has actually exited.
        Application.Run(window);
        return (work!.GetAwaiter().GetResult(), shown, ticks);
    }

    private static string? Value(string[] args, string key)
    {
        int index = Array.IndexOf(args, key);
        return index >= 0 && index + 1 < args.Length ? args[index + 1] : null;
    }

    private static bool IsAdministrator()
    {
        using var identity = WindowsIdentity.GetCurrent();
        return new WindowsPrincipal(identity).IsInRole(WindowsBuiltInRole.Administrator);
    }

    private static Stream Resource(string name) =>
        Assembly.GetExecutingAssembly().GetManifestResourceStream(name)
        ?? throw new InvalidDataException("Installer resource missing: " + name);

    private static void Extract(string root)
    {
        Directory.CreateDirectory(root);
        using (var payload = Resource("payload.zip"))
        using (var archive = new ZipArchive(payload, ZipArchiveMode.Read))
            archive.ExtractToDirectory(root);
        // Validate the embedded package before the GUI/discovery scripts execute.
        string sums = Path.Combine(root, "SHA256SUMS.txt");
        if (!File.Exists(sums)) throw new InvalidDataException("Installer payload checksums missing.");
        if (File.Exists(sums))
        {
            string prefix = Path.GetFullPath(root) + Path.DirectorySeparatorChar;
            var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (string line in File.ReadLines(sums))
            {
                if (line.Length < 67 || line.Substring(64, 2) != "  ")
                    throw new InvalidDataException("Invalid payload checksum.");
                string path = Path.GetFullPath(Path.Combine(root, line[66..]));
                if (!path.StartsWith(prefix, StringComparison.OrdinalIgnoreCase) || !seen.Add(path))
                    throw new InvalidDataException("Unsafe or duplicate payload checksum.");
                using var input = File.OpenRead(path);
                if (!Convert.ToHexString(SHA256.HashData(input)).Equals(line[..64], StringComparison.OrdinalIgnoreCase))
                    throw new InvalidDataException("Installer payload checksum mismatch: " + line[66..]);
            }
            if (Directory.EnumerateFiles(root, "*", SearchOption.AllDirectories).Count() != seen.Count + 1)
                throw new InvalidDataException("Installer payload file count mismatch.");
        }
        string banner = Path.Combine(root, "assets", "banner", "penumbra-vr-framework.png");
        if (!File.Exists(banner))
        {
            Directory.CreateDirectory(Path.GetDirectoryName(banner)!);
            using var resource = Resource("banner.png");
            using var output = File.Create(banner);
            resource.CopyTo(output);
        }
    }

    private static int RunGui(string root, string language, string ready)
    {
        string powershell = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System),
            "WindowsPowerShell", "v1.0", "powershell.exe");
        var info = new ProcessStartInfo(powershell)
        { UseShellExecute = false, CreateNoWindow = true, WorkingDirectory = root,
            RedirectStandardError = true, RedirectStandardOutput = true };
        foreach (string arg in new[] { "-NoProfile", "-STA", "-ExecutionPolicy", "Bypass", "-File",
            Path.Combine(root, "tools", "Install-PenumbraFrameworkGui.ps1"), "-Language", language,
            "-ReadySignalPath", ready })
            info.ArgumentList.Add(arg);
        using var process = Process.Start(info) ?? throw new IOException("Could not start installer UI.");
        var output = process.StandardOutput.ReadToEndAsync();
        var error = process.StandardError.ReadToEndAsync();
        process.WaitForExit();
        Task.WaitAll(output, error);
        if (process.ExitCode != 0)
            throw new IOException("Installer UI failed. " + error.Result);
        return 0;
    }
}
