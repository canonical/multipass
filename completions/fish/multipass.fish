# Completions for the `multipass` command

set -l subcommands delete exec find get help info launch list ls mount purge \
    recover restart set sh shell start stop suspend transfer umount version

set -l output_formats "
csv
json
table
yaml"

set -l configuration_key_descriptions "
client.gui.autostart\t'A boolean that identifies whether or not the Multipass GUI should automatically launch on startup'
client.gui.hotkey\t'A keyboard shortcut for the GUI to open a shell into the primary instance'
client.primary-name\t'The name of the instance that is launched/recognized as primary'
local.driver\t'A string identifying the hypervisor back-end in use'"

set -l autostart_completions "
client.gui.autostart=on\t'Launch the Multipass GUI on startup'
client.gui.autostart=off\t'Don\'t launch the Multipass GUI on startup'
client.gui.autostart=yes\t'Launch the Multipass GUI on startup'
client.gui.autostart=no\t'Don\'t launch the Multipass GUI on startup'
client.gui.autostart=1\t'Launch the Multipass GUI on startup'
client.gui.autostart=0\t'Don\'t launch the Multipass GUI on startup'
client.gui.autostart=true\t'Launch the Multipass GUI on startup'
client.gui.autostart=false\t'Don\'t launch the Multipass GUI on startup'"

# OS-specific variables
set -l drivers
switch (uname)
    case Darwin
        set drivers "
        local.driver=hyperkit 
        local.driver=virtualbox"
    case Linux
        set drivers "
        local.driver=qemu 
        local.driver=libvirt"
    case CYGWIN* MINGW* MSYS*
        set drivers "
        local.driver=hyperv
        local.driver=virtualbox"
end

set -l windows_terminal_profile "
    client.apps.windows-terminal.profiles=primary\t'A string describing which profiles Multipass should enable in Windows Terminal'
    client.apps.windows-terminal.profiles=none\t'A string describing which profiles Multipass should enable in Windows Terminal'"

function __fish_print_multipass_instances --description 'Print a list of Multipass instances' -a select
    switch $select
        case all
            multipass ls |
                # skip past the header
                string match -ev 'Name *State' |
        		# strip the name of the instance from the first column of each row
                string replace -r '(.+?) .*' '$1'
        case deleted
            multipass ls |
                # skip past the header
                string match -ev 'Name *State' |
                # match any row which contains the word "Deleted"
                string match -e Deleted | 
        		# strip the name of the instance from the first column of each row
                string replace -r '(.+?) .*' '$1'
        case running
            multipass ls |
                # skip past the header
                string match -ev 'Name *State' |
                # match any row which contains the word "Running"
                string match -e Running |
        		# strip the name of the instance from the first column of each row
                string replace -r '(.+?) .*' '$1'
        # Down indicates the instance is either stopped or suspended
        case down
            multipass ls |
                # skip past the header
                string match -ev 'Name *State' |
                # match any row which contains either the word "Stopped" or the word "Suspended"
                string match -er 'Stopped|Suspended' | 
        		# strip the name of the instance from the first column of each row
                string replace -r '(.+?) .*' '$1'
        # Available instances are those which have been not deleted
        case available
            multipass ls |
                # skip past the header
                string match -ev 'Name *State' |
                # skip any row which contains the word "Deleted"
                string match -ev Deleted | 
        		# strip the name of the instance from the first column of each row
                string replace -r '(.+?) .*' '$1'
    end
end

function __fish_print_multipass_available_instances_less_primary_instance --description  'Print a list of all Multipass instances which have not been deleted and are not the primary instance. This is useful for listing which instances may be set as the primary instance.'
	set -l primary_instance (multipass get client.primary-name)
    multipass ls |
        # skip past the header
        string match -ev 'Name *State' |
        # skip any row which contains the word "Deleted"
        string match -ev Deleted | 
        # strip the name of the instance from the first column of each row
        string replace -r '(.+?) .*' '$1' |
        # skip any occurrences of the name of the primary instance
        string match -ev $primary_instance
end

function __fish_print_multipass_remotes --description 'Print a list of all available Multipass remotes'

	# daily and release are standard remotes
	set -l remotes daily: release: \
		# obtain the available images from Multipass in the YAML format for simpler processing
	    (multipass find --format yaml |
			# skip any images which contain an empty remote
		    string match -ev 'remote: ""' |
		    # extract the name of each remote
		    string replace -fr ' *remote: ([[:graph:]]+)' '$1:')

	# separate out the items in the list with newlines instead of spaces
	string split ' ' $remotes |
		# sort the list and remove any duplicates
		sort -u
end

set -l images "
daily:default\t'The default image on the daily remote'
daily:devel\t'The latest development series image'
daily:lts\t'The daily build of the latest Long Term Support image'
default\t'The default image on the release remote'
devel\t'The latest development series image'
lts\t'The release version of the latest Long Term Support image'
release:default\t'The default image on the release remote'
release:lts\t'The release version of the latest Long Term Support image'"

# General options
complete -c multipass -f -s h -l help -d 'Print Usage'
complete -c multipass -f -s v -l verbose -d "Increase logging verbosity. Repeat the 'v' in the short option for more detail."

# Delete
complete -c multipass -f -n "not __fish_seen_subcommand_from $subcommands" -a delete -d 'Delete instances'
complete -c multipass -f -n "__fish_seen_subcommand_from delete" -l all -d 'Delete all instances'
complete -c multipass -f -n "__fish_seen_subcommand_from delete" -s p -l purge -d 'Purge instances immediately'
complete -c multipass -f -n '__fish_seen_subcommand_from delete' -a '(__fish_print_multipass_instances available)' -d 'Instance'

# Exec
complete -c multipass -f -n "not __fish_seen_subcommand_from $subcommands" -a exec -d 'Run a command on an instance'
complete -c multipass -f -n '__fish_seen_subcommand_from exec && __fish_is_token_n 3' -a '(__fish_print_multipass_instances running)' -d 'Instance'
complete -c multipass -f -n "__fish_seen_subcommand_from exec && __fish_is_token_n 4" -a '(__fish_complete_command)' -d 'Command'

# Find
# todo: Add completions for all available images by parsing data from the find subcommand.
complete -c multipass -f -n "not __fish_seen_subcommand_from $subcommands" -a find -d 'Display available images to create instances from'
complete -c multipass -f -n "__fish_seen_subcommand_from find" -l show-unsupported -d 'Show unsupported cloud images as well'
complete -c multipass -x -n "__fish_seen_subcommand_from find" -l format -a "$output_formats" -d 'Output list in the requested format'
complete -c multipass -f -n "__fish_seen_subcommand_from find" -a '(__fish_print_multipass_remotes)' -d 'Remote'
complete -c multipass -f -n "__fish_seen_subcommand_from find" -a "$images" -d 'Image'

# Get
complete -c multipass -f -n "not __fish_seen_subcommand_from $subcommands" -a get -d 'Get a configuration setting'
complete -c multipass -f -n "__fish_seen_subcommand_from get" -l raw -d 'Output in raw format'
complete -c multipass -f -n "__fish_seen_subcommand_from get" -a "$configuration_key_descriptions" -d 'Key'

# Help
complete -c multipass -f -n "not __fish_seen_subcommand_from $subcommands" -a help -d 'Display help about a command'
complete -c multipass -f -n '__fish_seen_subcommand_from help' -a "$subcommands" -d 'Subcommand'

# Info
complete -c multipass -f -n "not __fish_seen_subcommand_from $subcommands" -a info -d 'Display information about instances'
complete -c multipass -f -n "__fish_seen_subcommand_from info" -l all -d 'Display info for all instances'
complete -c multipass -x -n "__fish_seen_subcommand_from info" -l format -a "$output_formats" -d 'Output list in the requested format'
complete -c multipass -f -n '__fish_seen_subcommand_from info' -a '(__fish_print_multipass_instances all)' -d 'Instance'

# Launch
# todo: Add completions for all available images by parsing data from the find subcommand.
complete -c multipass -f -n "not __fish_seen_subcommand_from $subcommands" -a launch -d 'Create and start an Ubuntu instance'
complete -c multipass -f -n "__fish_seen_subcommand_from launch" -s c -l cpus -d 'Number of CPUs to allocate'
complete -c multipass -f -n "__fish_seen_subcommand_from launch" -s d -l disk -d 'Disk space to allocate'
complete -c multipass -f -n "__fish_seen_subcommand_from launch" -s m -l mem -d 'Amount of memory to allocate'
complete -c multipass -f -n "__fish_seen_subcommand_from launch" -s n -l name -d 'Name for the instance'
complete -c multipass -n "__fish_seen_subcommand_from launch" -l cloud-init -d 'Path to a user-data cloud-init configuration'
complete -c multipass -f -n "__fish_seen_subcommand_from find" -a '(__fish_print_multipass_remotes)' -d 'Remote'
complete -c multipass -f -n "__fish_seen_subcommand_from launch" -a "$images" -d 'Image'

# List
complete -c multipass -f -n "not __fish_seen_subcommand_from $subcommands" -a ls -d 'List all available instances'
complete -c multipass -f -n "not __fish_seen_subcommand_from $subcommands" -a list -d 'List all available instances'
complete -c multipass -x -n "__fish_seen_subcommand_from ls list" -l format -a "$output_formats" -d 'Output list in the requested format'

# Mount
# todo: Complete instance names and directories.
complete -c multipass -f -n "not __fish_seen_subcommand_from $subcommands" -a mount -d 'Mount a local directory in the instance'
complete -c multipass -f -n "__fish_seen_subcommand_from mount" -s g -l gid-map -d 'A mapping of group IDs for use in the mount'
complete -c multipass -f -n "__fish_seen_subcommand_from mount" -s u -l uid-map -d 'A mapping of user IDs for use in the mount'
complete -c multipass -n '__fish_seen_subcommand_from mount' -a '(__fish_print_multipass_instances running)' -d 'Instance'

# Purge
complete -c multipass -f -n "not __fish_seen_subcommand_from $subcommands" -a purge -d 'Purge all deleted instances permanently'

# Recover
complete -c multipass -f -n "not __fish_seen_subcommand_from $subcommands" -a recover -d 'Recover deleted instances'
complete -c multipass -f -n "__fish_seen_subcommand_from recover" -l all -d 'Recover all instances'
complete -c multipass -f -n '__fish_seen_subcommand_from recover' -a '(__fish_print_multipass_instances deleted)' -d 'Instance'

# Restart
complete -c multipass -f -n "not __fish_seen_subcommand_from $subcommands" -a restart -d 'Restart instances'
complete -c multipass -f -n "__fish_seen_subcommand_from restart" -l all -d 'Restart all instances'
complete -c multipass -f -n '__fish_seen_subcommand_from restart' -a '(__fish_print_multipass_instances running)' -d 'Instance'

# Set
complete -c multipass -f -n "not __fish_seen_subcommand_from $subcommands" -a set -d 'Set a configuration setting'
complete -c multipass -f -n "__fish_seen_subcommand_from set" -a "$autostart_completions"
complete -c multipass -f -n "__fish_seen_subcommand_from set" -a "\"client.primary-name=\"(__fish_print_multipass_available_instances_less_primary_instance)" -d 'Set the primary instance'
complete -c multipass -f -n "__fish_seen_subcommand_from set" -a "$drivers" -d 'Set the hypervisor back-end to use'
complete -c multipass -f -n "__fish_seen_subcommand_from set" -a "client.gui.hotkey=" -d 'Set a keyboard shortcut for the GUI to open a shell into the primary instance'
switch (uname)
	case 'CYGWIN*' 'MINGW*' 'MSYS*'
		complete -c multipass -f -n "__fish_seen_subcommand_from set" -a "$windows_terminal_profile" -d 'A string describing which profiles Multipass should enable in Windows Terminal'
end

# Shell
complete -c multipass -f -n "not __fish_seen_subcommand_from $subcommands" -a sh -d 'Open a shell on a running instance'
complete -c multipass -f -n "not __fish_seen_subcommand_from $subcommands" -a shell -d 'Open a shell on a running instance'
complete -c multipass -f -n '__fish_seen_subcommand_from sh shell' -a '(__fish_print_multipass_instances running)' -d 'Instance'

# Start
complete -c multipass -f -n "not __fish_seen_subcommand_from $subcommands" -a start -d 'Start instances'
complete -c multipass -f -n "__fish_seen_subcommand_from start" -l all -d 'Start all instances'
complete -c multipass -f -n '__fish_seen_subcommand_from start' -a '(__fish_print_multipass_instances down)' -d 'Instance'

# Stop
complete -c multipass -f -n "not __fish_seen_subcommand_from $subcommands" -a stop -d 'Stop running instances'
complete -c multipass -f -n "__fish_seen_subcommand_from stop" -l all -d 'Stop all instances'
complete -c multipass -x -n "__fish_seen_subcommand_from stop" -s t -l time -d 'Time from now, in minutes, to delay shutdown of the instance'
complete -c multipass -f -n "__fish_seen_subcommand_from stop" -s c -l cancel -d 'Cancel a pending delayed shutdown'
complete -c multipass -f -n '__fish_seen_subcommand_from stop' -a '(__fish_print_multipass_instances running)' -d 'Instance'

# Suspend
complete -c multipass -f -n "not __fish_seen_subcommand_from $subcommands" -a suspend -d 'Suspend running instances'
complete -c multipass -f -n "__fish_seen_subcommand_from suspend" -l all -d 'Suspend all instances'
complete -c multipass -f -n '__fish_seen_subcommand_from suspend' -a '(__fish_print_multipass_instances running)' -d 'Instance'

# Transfer
# todo: Complete transfers between instances and directories / files on those instances, just like what is done for the scp command.
complete -c multipass -f -n "not __fish_seen_subcommand_from $subcommands" -a transfer -d 'Transfer files between the host and instances'
complete -c multipass -n '__fish_seen_subcommand_from transfer' -a '(__fish_print_multipass_instances running)' -d 'Instance'

# Umount
# todo: Complete instance names and directories.
complete -c multipass -f -n "not __fish_seen_subcommand_from $subcommands" -a umount -d 'Unmount a directory from an instance'
complete -c multipass -n '__fish_seen_subcommand_from umount' -a '(__fish_print_multipass_instances running)' -d 'Instance'

# Version
complete -c multipass -f -n "not __fish_seen_subcommand_from $subcommands" -a version -d 'Show version details'
