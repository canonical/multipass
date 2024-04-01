using Microsoft.Win32;
using System;
using System.IO;
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

        [CustomAction]
        public static ActionResult UninstallOldMP(Session session)
        {
            session.Log("Begin UninstallOldMP");
            var properties = session.CustomActionData.ToString().Split('|');

            string uninstallString = string.IsNullOrEmpty(properties[0]) ? properties[1] : properties[0];

            ProcessStartInfo psi = new ProcessStartInfo
            {
                FileName = "powershell.exe",
                Arguments = @"-Command ""Start-Process 'C:\Program Files\Multipass\Uninstall.exe' /S -Wait""",
                RedirectStandardError = true,
                UseShellExecute = false,
                CreateNoWindow = true
            };

            Process process = new Process();
            process.StartInfo = psi;
            process.Start();
            process.WaitForExit();

            if (process.ExitCode != 0)
            {
                session.Log($"Failed to uninstall Multipass: {process.StandardError.ReadToEnd().Trim()}");
                return ActionResult.Failure;
            }

            string keyPath = @"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Multipass";
            string keyPath2 = @"SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\Multipass";
            RegistryKey regKey = Registry.LocalMachine.OpenSubKey(keyPath);
            RegistryKey regKey2 = Registry.LocalMachine.OpenSubKey(keyPath2);
            if (regKey != null || regKey2 != null)
            {
                return ActionResult.Failure;
            }

            return ActionResult.Success;
        }
    }
}
