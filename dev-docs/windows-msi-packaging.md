# MSI Packaging for Windows Deployment

### Building:

`ninja package` will build the Multipass installer, but will place the package under `<build-dir>\packages\en-US\`. The contents of the msi package can be inspected with [Orca](https://learn.microsoft.com/en-us/windows/win32/msi/orca-exe). MSI packages are at their core a relational database. Actions and properties are placed in appropriate tables. Some tables of interest are the `InstallExecuteSequence` and `InstallUISequence` tables. Sorting by `Sequence` will also you to visualize the order of events/actions during the respective sequence.

### Installing:

The installer can be executed simply by double-clicking on it in the File Explorer. However, if you need more control or more verbose output, `msiexec` can be used. The command `msiexec /i <absolute path to the installer> /l*vx! <output.log>` will install the package with verbose logging and create `output.log` with the output. Uninstalling can be done similarly with the same command. Since `/i` is being used, the Windows Installer will give you the options of **Repair** or **Remove**. An explicit uninstall can be done with `msiexec /x`. Additionally, passing the `/Q` flag will execute the installer in silent mode (you must use a shell launched with administrator privileges).

Repairing is a unique feature available with msi packages that checks all component key paths and installs any components that are missing. If the installation files are corrupted, this should fix it.

Removing is the same as uninstalling except that there is no need for a separate uninstall script. The Windows Installer simply does the opposite of what it did during install for items that are defined in native WiX code. eg. Add file -> remove file, Install service -> delete service. Other actions that were implemented with custom actions must be undone with additional custom actions.

Public properties (properties named in ALL CAPS) can also be set from the command line. For example, if you need to specify the driver, environment variable, or install directory during a silent install, you can do so by appending `DRIVER=<hyperv|virtualbox>` or `ENVIRONMENT=<user|system|none>` or `INSTALLFOLDER=<path>`. Properties must be public in order for the UI/client phase of the installation to access them.

### Changes from the NSIS installer

#### Product Versions

`msi` packages are installed with the [Windows Installer](https://learn.microsoft.com/en-us/windows/win32/msi/windows-installer-portal) and are subject to stricter rules for [product versions](https://learn.microsoft.com/en-us/windows/win32/msi/productversion). Versions must now conform to the style `[0-255].[0-255].[0-65535].[0-65535]`, of which the Windows Installer uses the first 3 fields for detecting upgrades/downgrades. We will continue to use the current version format internally within Multipass, but pull out the first three numbers from the version string as well as the number of commits since the preceding tag as given by 'git describe' (if it exists) for the version displayed in ARP.

#### Localized packages

WiX supports localized strings in order to appropriately display text in the user's locale (eg. en-US, ja-JP, etc). WiX will then build a package for every `WixLocalization` element defined. See `packaging/windows/wix/Package.en-US.wxl`. For this reason, built packages are placed in the `packages` subdirectory under the build directory with an additional sub-subdirectory for each locale.

#### Driver changes/Installation failures

Due to the nature of how the Windows Installer performs installs and the complication of performing a silent installation, the install behavior of setting the driver has had to change.

Previously, if the installer failed to enable the Hyper-V feature, a message box would appear asking the user if they would like to use VirtualBox instead. This functionality is no longer possible.
- Firstly, during a silent installation, it would impossible to ask the user this.
- Secondly, since enabling (or even querying the status of) Hyper-V requires elevated privileges, it must take place during the execution phase of the installation during a deferred custom action (See https://learn.microsoft.com/en-us/windows/win32/msi/installation-mechanism for more info). Deferred custom actions like this cannot set installer properties such as the the driver selection property, meaning that subsequent custom actions (such as the action that pre-populates the Multipass service config file) would not be able to act on any user selection (See https://learn.microsoft.com/en-us/windows/win32/msi/obtaining-context-information-for-deferred-execution-custom-actions for more info).

> Property values that are set at the time the installation sequence is processed into script may be unavailable at the time of script execution. Only the following limited set of properties is always accessible to custom actions during script execution.

This leaves us with two options then:

1. Fail the installation if enabling Hyper-V fails (or if VirtualBox is not installed...)

Going this route creates a problem with silent installations (such as from the Microsoft Store). Hyper-V is the default driver meaning that if enabling it failed (eg. on Windows Home), the user would be blocked from ever installing Multipass.

2. Ignore these driver selection failures during install and instead surface them through a user interface (CLI or GUI)

This is the better of the two options as the user will be unable to launch instances and get an appropriate error message from the Multipass service. Perhaps some improvements can be made in the CLI/GUI to let the user know that the hypervisor is unavailable.

### Package signing

For signing with WiX, see https://wixtoolset.org/docs/tools/signing/

WiX is only one way to build msi files and they are independent technologies. WiX has their own tool to sign files, but other tools can be used as well. SignTool is one such example that Canonical uses and can successfully sign exe and msi package types. See https://wiki.canonical.com/InformationInfrastructure/IS/WindowsBinarySigning/OEMCodeSigning for IS documentation.

### Testing checklist:

- [ ] Packaging/CI:
  - [ ] Product version automatically set through cmake/cpack
  - [ ] Output directory automatically set
  - [ ] All dependencies scraped

- [ ] Installation:
  - [ ] ARP (Control Panel -> Programs and Features)
    - [ ] Icon, Product Name, Publisher, Version set
    - [ ] Product details: Publisher, Product version, Help link, Support link set
  - [ ] Services
    - [ ] Service installed correctly (Name, Display Name, Description, Path to executable with correct arguments, Startup type, Status, Log on account, Recovery failure settings)
    - [ ] Hypervisor setting correctly populated in `C:\ProgramData\Multipass\multipassd.conf`
  - [ ] Event Viewer
    - [ ] Multipass logs show up
  - [ ] Install Directory
    - [ ] Correct directory and appropriate files (default is `C:\Program Files\Multipass')
    - [ ] Modifying installation directory works
  - [ ] Ubuntu Mono font installed
  - [ ] Environment variables
    - [ ] Correct location
    - [ ] `multipass` works from shell
  - [ ] CLI
    - [ ] Client certificates installed
      - [ ] Present in `<user>\AppData\Local\multipass` and `<user>\AppData\Local\multipass-client-certificates`
      - [ ] Key store created in `C:\ProgramData\Multipass\data\authenticated-
certs\multipass_client_certs.pem`
    - [ ] Connects to Multipass service
  - [ ] GUI
    - [ ] Shortcut is available in the Start Menu
    - [ ] Auto-starts after installation
    - [ ] Persists on system restart
    - [ ] Connects to Multipass service
  - [ ] Installer launch conditions
    - [ ] Windows April 2018 Update required
  - [ ] Hypervisors
    - [ ] Installer enables HyperV
    - [ ] Virtualbox is used if using Windows Home (hypervisor selection dialog is not shown)
  - [ ] Multipass icon present in Windows Terminal
  - [ ] Silent install (see Installation)
    - [ ] Specifying custom install directory works `INSTALLFOLDER=path\to\folder`
    - [ ] Specifying driver works `DRIVER=hyperv|virtualbox`
    - [ ] Specifying path works `ENVIRONMENT=user|system|none`

- [ ] Upgrade:
  - [ ] Multipass data is not removed (eg. instances persist)
  - [ ] Version in ARP updates
  - [ ] Upgrades from NSIS installer
    - [ ] Previous version of Multipass is uninstalled (eg. registry keys are removed)
    - [ ] Multipass data transfers over
  - [ ] Driver selection dialog is skipped
  - [ ] Silent upgrade works
  - [ ] Multipass service refreshed
  - [ ] Multipass GUI stopped

- [ ] Uninstall:
  - [ ] Multipass removed from ARP
  - [ ] Install directory is completely removed
  - [ ] Multipass data
    - [ ] UI asks user about removing data
    - [ ] Multipass data directory is removed
    - [ ] Instances are removed (eg. Hyper-V Manager, VirtualBox)
    - [ ] `multipass` and `multipass-client-certificates` are removed from `<User>\AppData\Local\`
  - [ ] Items added during installation are removed (eg. files, services, shortcuts, registry keys, fonts, etc)
    - [ ] Multipass service stopped
    - [ ] Multipass GUI stopped
  - [ ] Silent uninstall works
    - [ ] Removes Multipass data
    - [ ] `REMOVE_DATA=no` does not remove Multipass data
