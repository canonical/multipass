using System.Diagnostics;
using WixToolset.Dtf.WindowsInstaller;

namespace CustomActions
{
    public class CustomActions
    {
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
