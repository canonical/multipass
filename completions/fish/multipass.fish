# Copyright (C) Canonical, Ltd.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# Fish completion script for Multipass

# Helper functions for dynamic completions
function __multipass_instances
    set -l state_filter $argv[1]
    multipass list --format=csv --no-ipv4 2>/dev/null | tail -n +2 | while read -l line
        set -l parts (string split ',' $line)
        set -l name $parts[1]
        set -l status $parts[2]
        if test -z "$state_filter"; or test "$status" = "$state_filter"
            echo $name
        end
    end
end

function __multipass_instances_running
    __multipass_instances Running
end

function __multipass_instances_stopped
    __multipass_instances Stopped
end

function __multipass_instances_suspended
    __multipass_instances Suspended
end

function __multipass_instances_deleted
    __multipass_instances Deleted
end

function __multipass_instances_startable
    __multipass_instances Stopped
    __multipass_instances Suspended
end

function __multipass_instances_shellable
    __multipass_instances Running
    __multipass_instances Stopped
    __multipass_instances Suspended
end

function __multipass_instances_stoppable
    set -l has_force 0
    for word in (commandline -opc)
        if test "$word" = "--force" -o "$word" = "-f"
            set has_force 1
            break
        end
    end

    __multipass_instances Running
    if test $has_force -eq 1
        __multipass_instances Starting
        __multipass_instances Restarting
        __multipass_instances Suspending
        __multipass_instances Suspended
    end
end

function __multipass_snapshots
    set -l instance_filter $argv[1]
    multipass list --snapshots --format=csv 2>/dev/null | tail -n +2 | while read -l line
        set -l parts (string split ',' $line)
        set -l instance $parts[1]
        set -l snapshot $parts[2]
        if test -z "$instance_filter"; or test "$instance" = "$instance_filter"
            echo "$instance.$snapshot"
        end
    end
end

function __multipass_instances_and_snapshots
    __multipass_instances
    __multipass_snapshots
end

function __multipass_restorable_snapshots
    for instance in (multipass info --no-runtime-information --format=csv 2>/dev/null | tail -n +2 | awk -F',' '$2 == "Stopped" && $16 > 0 {print $1}')
        __multipass_snapshots $instance
    end
end

function __multipass_networks
    multipass networks --format=csv 2>/dev/null | tail -n +2 | cut -d',' -f1
end

function __multipass_settings_keys
    multipass get --keys 2>/dev/null
end

function __multipass_aliases_list
    multipass aliases --format=csv 2>/dev/null | tail -n +2 | cut -d',' -f1
end

function __multipass_needs_command
    set -l cmd (commandline -opc)
    if test (count $cmd) -eq 1
        return 0
    end
    return 1
end

function __multipass_using_command
    set -l cmd (commandline -opc)
    if test (count $cmd) -gt 1
        if test $argv[1] = $cmd[2]
            return 0
        end
    end
    return 1
end

# Disable file completion by default
complete -c multipass -f

# Global options
complete -c multipass -s h -l help -d 'Display help information'
complete -c multipass -s v -l verbose -d 'Increase logging verbosity'

# Commands
complete -c multipass -n __multipass_needs_command -a alias -d 'Create an alias to run a command in an instance'
complete -c multipass -n __multipass_needs_command -a aliases -d 'List available aliases'
complete -c multipass -n __multipass_needs_command -a authenticate -d 'Authenticate client'
complete -c multipass -n __multipass_needs_command -a clone -d 'Clone an instance'
complete -c multipass -n __multipass_needs_command -a delete -d 'Delete instances and snapshots'
complete -c multipass -n __multipass_needs_command -a exec -d 'Run a command in an instance'
complete -c multipass -n __multipass_needs_command -a find -d 'Search available images'
complete -c multipass -n __multipass_needs_command -a get -d 'Get a configuration setting'
complete -c multipass -n __multipass_needs_command -a help -d 'Display help about a command'
complete -c multipass -n __multipass_needs_command -a info -d 'Display information about instances'
complete -c multipass -n __multipass_needs_command -a launch -d 'Create and start an Ubuntu instance'
complete -c multipass -n __multipass_needs_command -a list -d 'List all available instances'
complete -c multipass -n __multipass_needs_command -a ls -d 'List all available instances (alias)'
complete -c multipass -n __multipass_needs_command -a mount -d 'Mount a local directory in the instance'
complete -c multipass -n __multipass_needs_command -a networks -d 'List available network interfaces'
complete -c multipass -n __multipass_needs_command -a prefer -d 'Switch the current alias context'
complete -c multipass -n __multipass_needs_command -a purge -d 'Purge all deleted instances permanently'
complete -c multipass -n __multipass_needs_command -a recover -d 'Recover deleted instances'
complete -c multipass -n __multipass_needs_command -a restart -d 'Restart instances'
complete -c multipass -n __multipass_needs_command -a restore -d 'Restore an instance from a snapshot'
complete -c multipass -n __multipass_needs_command -a set -d 'Set a configuration setting'
complete -c multipass -n __multipass_needs_command -a shell -d 'Open a shell in an instance'
complete -c multipass -n __multipass_needs_command -a sh -d 'Open a shell in an instance (alias)'
complete -c multipass -n __multipass_needs_command -a connect -d 'Open a shell in an instance (alias)'
complete -c multipass -n __multipass_needs_command -a snapshot -d 'Take a snapshot of an instance'
complete -c multipass -n __multipass_needs_command -a start -d 'Start instances'
complete -c multipass -n __multipass_needs_command -a stop -d 'Stop running instances'
complete -c multipass -n __multipass_needs_command -a suspend -d 'Suspend running instances'
complete -c multipass -n __multipass_needs_command -a transfer -d 'Transfer files between host and instance'
complete -c multipass -n __multipass_needs_command -a "copy-files" -d 'Transfer files (alias)'
complete -c multipass -n __multipass_needs_command -a umount -d 'Unmount a directory from an instance'
complete -c multipass -n __multipass_needs_command -a unmount -d 'Unmount a directory (alias)'
complete -c multipass -n __multipass_needs_command -a unalias -d 'Remove aliases'
complete -c multipass -n __multipass_needs_command -a version -d 'Show version details'

# Also complete user-defined aliases as commands
complete -c multipass -n __multipass_needs_command -a '(__multipass_aliases_list)' -d 'User-defined alias'

# alias command
complete -c multipass -n '__multipass_using_command alias' -l no-map-working-directory -d 'Do not map working directory'
complete -c multipass -n '__multipass_using_command alias' -a '(__multipass_instances)' -d 'Instance'

# aliases command
complete -c multipass -n '__multipass_using_command aliases' -s f -l format -xa 'table json csv yaml' -d 'Output format'

# clone command
complete -c multipass -n '__multipass_using_command clone' -s n -l name -d 'Name for the cloned instance'
complete -c multipass -n '__multipass_using_command clone' -a '(__multipass_instances_stopped)' -d 'Instance'

# delete command
complete -c multipass -n '__multipass_using_command delete' -l all -d 'Delete all instances'
complete -c multipass -n '__multipass_using_command delete' -s p -l purge -d 'Purge instances immediately'
complete -c multipass -n '__multipass_using_command delete' -a '(__multipass_instances_and_snapshots)' -d 'Instance or snapshot'

# exec command
complete -c multipass -n '__multipass_using_command exec' -s d -l working-directory -rF -d 'Working directory'
complete -c multipass -n '__multipass_using_command exec' -s n -l no-map-working-directory -d 'Do not map working directory'
complete -c multipass -n '__multipass_using_command exec' -a '(__multipass_instances_running)' -d 'Instance'

# find command
complete -c multipass -n '__multipass_using_command find' -l show-unsupported -d 'Show unsupported images'
complete -c multipass -n '__multipass_using_command find' -l force-update -d 'Force image list update'
complete -c multipass -n '__multipass_using_command find' -s f -l format -xa 'table json csv yaml' -d 'Output format'

# get command
complete -c multipass -n '__multipass_using_command get' -l raw -d 'Output raw values'
complete -c multipass -n '__multipass_using_command get' -l keys -d 'List available keys'
complete -c multipass -n '__multipass_using_command get' -a '(__multipass_settings_keys)' -d 'Setting key'

# help command
complete -c multipass -n '__multipass_using_command help' -a 'alias aliases authenticate clone delete exec find get help info launch list mount networks purge recover restart restore set shell snapshot start stop suspend transfer umount unalias version' -d 'Command'

# info command
complete -c multipass -n '__multipass_using_command info' -s f -l format -xa 'table json csv yaml' -d 'Output format'
complete -c multipass -n '__multipass_using_command info' -l snapshots -d 'Show snapshot information'
complete -c multipass -n '__multipass_using_command info' -l no-runtime-information -d 'Skip runtime info'
complete -c multipass -n '__multipass_using_command info' -a '(__multipass_instances_and_snapshots)' -d 'Instance or snapshot'

# launch command
complete -c multipass -n '__multipass_using_command launch' -s c -l cpus -d 'Number of CPUs'
complete -c multipass -n '__multipass_using_command launch' -s d -l disk -d 'Disk space (e.g., 10G)'
complete -c multipass -n '__multipass_using_command launch' -s m -l memory -d 'Memory size (e.g., 1G)'
complete -c multipass -n '__multipass_using_command launch' -s n -l name -d 'Instance name'
complete -c multipass -n '__multipass_using_command launch' -l cloud-init -rF -d 'Cloud-init config file'
complete -c multipass -n '__multipass_using_command launch' -l network -xa '(__multipass_networks) bridged' -d 'Network specification'
complete -c multipass -n '__multipass_using_command launch' -l bridged -d 'Use bridged network'
complete -c multipass -n '__multipass_using_command launch' -l mount -rF -d 'Mount specification'
complete -c multipass -n '__multipass_using_command launch' -s t -l timeout -d 'Timeout in seconds'

# list command
complete -c multipass -n '__multipass_using_command list' -s f -l format -xa 'table json csv yaml' -d 'Output format'
complete -c multipass -n '__multipass_using_command list' -l snapshots -d 'List snapshots'
complete -c multipass -n '__multipass_using_command list' -l no-ipv4 -d 'Omit IPv4 column'
complete -c multipass -n '__multipass_using_command ls' -s f -l format -xa 'table json csv yaml' -d 'Output format'
complete -c multipass -n '__multipass_using_command ls' -l snapshots -d 'List snapshots'
complete -c multipass -n '__multipass_using_command ls' -l no-ipv4 -d 'Omit IPv4 column'

# mount command
complete -c multipass -n '__multipass_using_command mount' -s g -l gid-map -d 'GID mapping'
complete -c multipass -n '__multipass_using_command mount' -s u -l uid-map -d 'UID mapping'
complete -c multipass -n '__multipass_using_command mount' -s t -l type -xa 'classic native' -d 'Mount type'
complete -c multipass -n '__multipass_using_command mount' -rF -d 'Source directory'
complete -c multipass -n '__multipass_using_command mount' -a '(__multipass_instances_shellable)' -d 'Instance'

# networks command
complete -c multipass -n '__multipass_using_command networks' -s f -l format -xa 'table json csv yaml' -d 'Output format'

# recover command
complete -c multipass -n '__multipass_using_command recover' -l all -d 'Recover all deleted instances'
complete -c multipass -n '__multipass_using_command recover' -a '(__multipass_instances_deleted)' -d 'Deleted instance'

# restart command
complete -c multipass -n '__multipass_using_command restart' -l all -d 'Restart all instances'
complete -c multipass -n '__multipass_using_command restart' -s t -l timeout -d 'Timeout in seconds'
complete -c multipass -n '__multipass_using_command restart' -a '(__multipass_instances_running)' -d 'Instance'

# restore command
complete -c multipass -n '__multipass_using_command restore' -l destructive -d 'Discard current state'
complete -c multipass -n '__multipass_using_command restore' -a '(__multipass_restorable_snapshots)' -d 'Snapshot'

# set command
complete -c multipass -n '__multipass_using_command set' -a '(__multipass_settings_keys)' -d 'Setting key'

# shell/sh/connect command
complete -c multipass -n '__multipass_using_command shell' -s t -l timeout -d 'Timeout in seconds'
complete -c multipass -n '__multipass_using_command shell' -a '(__multipass_instances_shellable)' -d 'Instance'
complete -c multipass -n '__multipass_using_command sh' -s t -l timeout -d 'Timeout in seconds'
complete -c multipass -n '__multipass_using_command sh' -a '(__multipass_instances_shellable)' -d 'Instance'
complete -c multipass -n '__multipass_using_command connect' -s t -l timeout -d 'Timeout in seconds'
complete -c multipass -n '__multipass_using_command connect' -a '(__multipass_instances_shellable)' -d 'Instance'

# snapshot command
complete -c multipass -n '__multipass_using_command snapshot' -s n -l name -d 'Snapshot name'
complete -c multipass -n '__multipass_using_command snapshot' -s c -l comment -d 'Snapshot comment'
complete -c multipass -n '__multipass_using_command snapshot' -a '(__multipass_instances_stopped)' -d 'Instance'

# start command
complete -c multipass -n '__multipass_using_command start' -l all -d 'Start all instances'
complete -c multipass -n '__multipass_using_command start' -s t -l timeout -d 'Timeout in seconds'
complete -c multipass -n '__multipass_using_command start' -a '(__multipass_instances_startable)' -d 'Instance'

# stop command
complete -c multipass -n '__multipass_using_command stop' -l all -d 'Stop all instances'
complete -c multipass -n '__multipass_using_command stop' -s t -l time -d 'Delay shutdown by time'
complete -c multipass -n '__multipass_using_command stop' -s c -l cancel -d 'Cancel scheduled stop'
complete -c multipass -n '__multipass_using_command stop' -s f -l force -d 'Force stop'
complete -c multipass -n '__multipass_using_command stop' -a '(__multipass_instances_stoppable)' -d 'Instance'

# suspend command
complete -c multipass -n '__multipass_using_command suspend' -l all -d 'Suspend all instances'
complete -c multipass -n '__multipass_using_command suspend' -a '(__multipass_instances_running)' -d 'Instance'

# transfer/copy-files command
complete -c multipass -n '__multipass_using_command transfer' -s r -l recursive -d 'Transfer directories recursively'
complete -c multipass -n '__multipass_using_command transfer' -s p -l parents -d 'Make parent directories as needed'
complete -c multipass -n '__multipass_using_command transfer' -rF -d 'Source or destination'
complete -c multipass -n '__multipass_using_command copy-files' -s r -l recursive -d 'Transfer directories recursively'
complete -c multipass -n '__multipass_using_command copy-files' -s p -l parents -d 'Make parent directories as needed'
complete -c multipass -n '__multipass_using_command copy-files' -rF -d 'Source or destination'

# umount/unmount command
complete -c multipass -n '__multipass_using_command umount' -a '(__multipass_instances)' -d 'Instance'
complete -c multipass -n '__multipass_using_command unmount' -a '(__multipass_instances)' -d 'Instance'

# unalias command
complete -c multipass -n '__multipass_using_command unalias' -l all -d 'Remove all aliases'
complete -c multipass -n '__multipass_using_command unalias' -a '(__multipass_aliases_list)' -d 'Alias'

# prefer command
complete -c multipass -n '__multipass_using_command prefer' -a '(__multipass_aliases_list)' -d 'Alias context'

# version command
complete -c multipass -n '__multipass_using_command version' -s f -l format -xa 'table json csv yaml' -d 'Output format'
