using System.Threading;
using System.Diagnostics;
using WixToolset.Dtf.WindowsInstaller;

namespace CustomActions
{
    public class CustomActions
    {
        [CustomAction]
        public static ActionResult CheckHyperV(Session session)
        {
            session.Log("Begin EnableHyperVAction");

            ProcessStartInfo psi = new ProcessStartInfo
            {
                FileName = "powershell",
                Arguments = "Get-WindowsOptionalFeature -Online -FeatureName Microsoft-Hyper-V-All -ErrorAction SilentlyContinue | Select-Object -ExpandProperty State",
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                UseShellExecute = false,
                CreateNoWindow = true
            };

            Process process = Process.Start(psi);
            process.WaitForExit();

            if (process.ExitCode == 0)
                session["HYPER_V_STATE"] = process.StandardOutput.ReadToEnd().Trim();
            else if (process.ExitCode != 0)
                session.Log($"Error enabling Hyper-V: {process.StandardError.ReadToEnd().Trim()}");

            return ActionResult.Success;
        }

        [CustomAction]
        public static ActionResult EnableHyperV(Session session)
        {
            session.Log("Begin EnableHyperVAction");

            ProcessStartInfo psi = new ProcessStartInfo
            {
                FileName = "powershell",
                Arguments = "Enable-WindowsOptionalFeature -Online -FeatureName Microsoft-Hyper-V-All -All -NoRestart",
                RedirectStandardError = true,
                UseShellExecute = false,
                CreateNoWindow = true
            };

            Process process = Process.Start(psi);
            process.WaitForExit();

            if (process.ExitCode != 0)
            {
                session.Log($"Error enabling Hyper-V: {process.StandardError.ReadToEnd().Trim()}");
                session["HYPER_V_STATE"] = "Failure";
            }

            return ActionResult.Success;
        }

        [CustomAction]
        public static ActionResult SetDriver(Session session)
        {
            session.Log("Begin SetDriver");
            string command = $"\"\"{session["INSTALLFOLDER"]}bin\\multipass.exe\" set local.driver={session["DRIVER"]}\"";

            ProcessStartInfo psi = new ProcessStartInfo
            {
                FileName = "cmd.exe",
                Arguments = $"/c {command}",
                RedirectStandardError = true,
                UseShellExecute = false,
                CreateNoWindow = true
            };
            session.Log($"{psi.Arguments}");

            Process process = new Process();
            process.StartInfo = psi;

            int i = 0;
            int maxRetries = 30;
            while (i < maxRetries)
            {
                process.Start();
                process.WaitForExit();

                if (process.ExitCode == 0)
                    break;

                session.Log("Failed to set driver. Will try again in 1 second");
                session.Log($"Command error: {process.StandardError.ReadToEnd().Trim()}");
                Thread.Sleep(1000);
                i++;
            }

            if (i >= maxRetries)
            {
                session.Log($"Could not successfully set driver to {session["DRIVER"]}");
                return ActionResult.Failure;
            }

            return ActionResult.Success;
        }
    }
}
