using System.Diagnostics;
using WixToolset.Dtf.WindowsInstaller;

namespace CustomActions
{
    public class CustomActions
    {
        [CustomAction]
        public static ActionResult CheckHyperV(Session session)
        {
            session.Log("Begin CheckHyperV");

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

            if (process.ExitCode != 0)
            {
                session.Log($"Error checking Hyper-V status: {process.StandardError.ReadToEnd().Trim()}");
                return ActionResult.Failure;
            }

            return ActionResult.Success;
        }

        [CustomAction]
        public static ActionResult EnableHyperV(Session session)
        {
            session.Log("Begin EnableHyperV");

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
                return ActionResult.Failure;
            }

            return ActionResult.Success;
        }

        [CustomAction]
        public static ActionResult AskRemoveData(Session session)
        {
            session.Log("Begin AskRemoveData");

            Record record = new Record();
            record.FormatString = session["RemoveDataText"];
            MessageResult result = session.Message(InstallMessage.Error | (InstallMessage)MessageButtons.YesNo | (InstallMessage)MessageDefaultButton.Button2, record);

            if (result == MessageResult.Yes)
                session["REMOVE_DATA"] = "YES";
            else if (result == MessageResult.No)
                session["REMOVE_DATA"] = "NO";

            return ActionResult.Success;
        }
    }
}
