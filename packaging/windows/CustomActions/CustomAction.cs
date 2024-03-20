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
    }
}
