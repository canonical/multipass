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

# PowerShell completion script for Multipass

using namespace System.Management.Automation
using namespace System.Management.Automation.Language

# Helper functions for dynamic completions
function Get-MultipassInstances {
    param([string]$StateFilter = "")

    try {
        $output = multipass list --format=csv --no-ipv4 2>$null
        if ($LASTEXITCODE -ne 0) { return @() }

        $lines = $output -split "`n" | Select-Object -Skip 1
        $instances = @()

        foreach ($line in $lines) {
            if ([string]::IsNullOrWhiteSpace($line)) { continue }
            $parts = $line -split ','
            $name = $parts[0].Trim()
            $status = $parts[1].Trim()

            if ([string]::IsNullOrEmpty($StateFilter) -or $status -eq $StateFilter) {
                $instances += $name
            }
        }
        return $instances
    }
    catch {
        return @()
    }
}

function Get-MultipassSnapshots {
    param([string]$InstanceFilter = "")

    try {
        $output = multipass list --snapshots --format=csv 2>$null
        if ($LASTEXITCODE -ne 0) { return @() }

        $lines = $output -split "`n" | Select-Object -Skip 1
        $snapshots = @()

        foreach ($line in $lines) {
            if ([string]::IsNullOrWhiteSpace($line)) { continue }
            $parts = $line -split ','
            $instance = $parts[0].Trim()
            $snapshot = $parts[1].Trim()

            if ([string]::IsNullOrEmpty($InstanceFilter) -or $instance -eq $InstanceFilter) {
                $snapshots += "$instance.$snapshot"
            }
        }
        return $snapshots
    }
    catch {
        return @()
    }
}

function Get-MultipassNetworks {
    try {
        $output = multipass networks --format=csv 2>$null
        if ($LASTEXITCODE -ne 0) { return @() }

        $lines = $output -split "`n" | Select-Object -Skip 1
        $networks = @()

        foreach ($line in $lines) {
            if ([string]::IsNullOrWhiteSpace($line)) { continue }
            $parts = $line -split ','
            $networks += $parts[0].Trim()
        }
        return $networks
    }
    catch {
        return @()
    }
}

function Get-MultipassSettingsKeys {
    try {
        $output = multipass get --keys 2>$null
        if ($LASTEXITCODE -ne 0) { return @() }
        return ($output -split "`n" | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    }
    catch {
        return @()
    }
}

function Get-MultipassAliases {
    try {
        $output = multipass aliases --format=csv 2>$null
        if ($LASTEXITCODE -ne 0) { return @() }

        $lines = $output -split "`n" | Select-Object -Skip 1
        $aliases = @()

        foreach ($line in $lines) {
            if ([string]::IsNullOrWhiteSpace($line)) { continue }
            $parts = $line -split ','
            $aliases += $parts[0].Trim()
        }
        return $aliases
    }
    catch {
        return @()
    }
}

# Command definitions with their options
$script:MultipassCommands = @{
    'alias' = @{
        Description = 'Create an alias to run a command in an instance'
        Options = @(
            @{ Name = '--no-map-working-directory'; Description = 'Do not map working directory' }
            @{ Name = '--help'; Short = '-h'; Description = 'Display help' }
            @{ Name = '--verbose'; Short = '-v'; Description = 'Increase verbosity' }
        )
    }
    'aliases' = @{
        Description = 'List available aliases'
        Options = @(
            @{ Name = '--format'; Short = '-f'; Description = 'Output format'; Values = @('table', 'json', 'csv', 'yaml') }
            @{ Name = '--help'; Short = '-h'; Description = 'Display help' }
            @{ Name = '--verbose'; Short = '-v'; Description = 'Increase verbosity' }
        )
    }
    'authenticate' = @{
        Description = 'Authenticate client'
        Options = @(
            @{ Name = '--help'; Short = '-h'; Description = 'Display help' }
            @{ Name = '--verbose'; Short = '-v'; Description = 'Increase verbosity' }
        )
    }
    'clone' = @{
        Description = 'Clone an instance'
        Options = @(
            @{ Name = '--name'; Short = '-n'; Description = 'Name for the cloned instance' }
            @{ Name = '--help'; Short = '-h'; Description = 'Display help' }
            @{ Name = '--verbose'; Short = '-v'; Description = 'Increase verbosity' }
        )
    }
    'delete' = @{
        Description = 'Delete instances and snapshots'
        Options = @(
            @{ Name = '--all'; Description = 'Delete all instances' }
            @{ Name = '--purge'; Short = '-p'; Description = 'Purge instances immediately' }
            @{ Name = '--help'; Short = '-h'; Description = 'Display help' }
            @{ Name = '--verbose'; Short = '-v'; Description = 'Increase verbosity' }
        )
    }
    'exec' = @{
        Description = 'Run a command in an instance'
        Options = @(
            @{ Name = '--working-directory'; Short = '-d'; Description = 'Working directory' }
            @{ Name = '--no-map-working-directory'; Short = '-n'; Description = 'Do not map working directory' }
            @{ Name = '--help'; Short = '-h'; Description = 'Display help' }
            @{ Name = '--verbose'; Short = '-v'; Description = 'Increase verbosity' }
        )
    }
    'find' = @{
        Description = 'Search available images'
        Options = @(
            @{ Name = '--show-unsupported'; Description = 'Show unsupported images' }
            @{ Name = '--force-update'; Description = 'Force image list update' }
            @{ Name = '--format'; Short = '-f'; Description = 'Output format'; Values = @('table', 'json', 'csv', 'yaml') }
            @{ Name = '--help'; Short = '-h'; Description = 'Display help' }
            @{ Name = '--verbose'; Short = '-v'; Description = 'Increase verbosity' }
        )
    }
    'get' = @{
        Description = 'Get a configuration setting'
        Options = @(
            @{ Name = '--raw'; Description = 'Output raw values' }
            @{ Name = '--keys'; Description = 'List available keys' }
            @{ Name = '--help'; Short = '-h'; Description = 'Display help' }
            @{ Name = '--verbose'; Short = '-v'; Description = 'Increase verbosity' }
        )
    }
    'help' = @{
        Description = 'Display help about a command'
        Options = @()
    }
    'info' = @{
        Description = 'Display information about instances'
        Options = @(
            @{ Name = '--format'; Short = '-f'; Description = 'Output format'; Values = @('table', 'json', 'csv', 'yaml') }
            @{ Name = '--snapshots'; Description = 'Show snapshot information' }
            @{ Name = '--no-runtime-information'; Description = 'Skip runtime info' }
            @{ Name = '--help'; Short = '-h'; Description = 'Display help' }
            @{ Name = '--verbose'; Short = '-v'; Description = 'Increase verbosity' }
        )
    }
    'launch' = @{
        Description = 'Create and start an Ubuntu instance'
        Options = @(
            @{ Name = '--cpus'; Short = '-c'; Description = 'Number of CPUs' }
            @{ Name = '--disk'; Short = '-d'; Description = 'Disk space (e.g., 10G)' }
            @{ Name = '--memory'; Short = '-m'; Description = 'Memory size (e.g., 1G)' }
            @{ Name = '--name'; Short = '-n'; Description = 'Instance name' }
            @{ Name = '--cloud-init'; Description = 'Cloud-init config file' }
            @{ Name = '--network'; Description = 'Network specification' }
            @{ Name = '--bridged'; Description = 'Use bridged network' }
            @{ Name = '--mount'; Description = 'Mount specification' }
            @{ Name = '--timeout'; Short = '-t'; Description = 'Timeout in seconds' }
            @{ Name = '--help'; Short = '-h'; Description = 'Display help' }
            @{ Name = '--verbose'; Short = '-v'; Description = 'Increase verbosity' }
        )
    }
    'list' = @{
        Description = 'List all available instances'
        Options = @(
            @{ Name = '--format'; Short = '-f'; Description = 'Output format'; Values = @('table', 'json', 'csv', 'yaml') }
            @{ Name = '--snapshots'; Description = 'List snapshots' }
            @{ Name = '--no-ipv4'; Description = 'Omit IPv4 column' }
            @{ Name = '--help'; Short = '-h'; Description = 'Display help' }
            @{ Name = '--verbose'; Short = '-v'; Description = 'Increase verbosity' }
        )
    }
    'mount' = @{
        Description = 'Mount a local directory in the instance'
        Options = @(
            @{ Name = '--gid-map'; Short = '-g'; Description = 'GID mapping' }
            @{ Name = '--uid-map'; Short = '-u'; Description = 'UID mapping' }
            @{ Name = '--type'; Short = '-t'; Description = 'Mount type'; Values = @('classic', 'native') }
            @{ Name = '--help'; Short = '-h'; Description = 'Display help' }
            @{ Name = '--verbose'; Short = '-v'; Description = 'Increase verbosity' }
        )
    }
    'networks' = @{
        Description = 'List available network interfaces'
        Options = @(
            @{ Name = '--format'; Short = '-f'; Description = 'Output format'; Values = @('table', 'json', 'csv', 'yaml') }
            @{ Name = '--help'; Short = '-h'; Description = 'Display help' }
            @{ Name = '--verbose'; Short = '-v'; Description = 'Increase verbosity' }
        )
    }
    'prefer' = @{
        Description = 'Switch the current alias context'
        Options = @(
            @{ Name = '--help'; Short = '-h'; Description = 'Display help' }
            @{ Name = '--verbose'; Short = '-v'; Description = 'Increase verbosity' }
        )
    }
    'purge' = @{
        Description = 'Purge all deleted instances permanently'
        Options = @(
            @{ Name = '--help'; Short = '-h'; Description = 'Display help' }
            @{ Name = '--verbose'; Short = '-v'; Description = 'Increase verbosity' }
        )
    }
    'recover' = @{
        Description = 'Recover deleted instances'
        Options = @(
            @{ Name = '--all'; Description = 'Recover all deleted instances' }
            @{ Name = '--help'; Short = '-h'; Description = 'Display help' }
            @{ Name = '--verbose'; Short = '-v'; Description = 'Increase verbosity' }
        )
    }
    'restart' = @{
        Description = 'Restart instances'
        Options = @(
            @{ Name = '--all'; Description = 'Restart all instances' }
            @{ Name = '--timeout'; Short = '-t'; Description = 'Timeout in seconds' }
            @{ Name = '--help'; Short = '-h'; Description = 'Display help' }
            @{ Name = '--verbose'; Short = '-v'; Description = 'Increase verbosity' }
        )
    }
    'restore' = @{
        Description = 'Restore an instance from a snapshot'
        Options = @(
            @{ Name = '--destructive'; Description = 'Discard current state' }
            @{ Name = '--help'; Short = '-h'; Description = 'Display help' }
            @{ Name = '--verbose'; Short = '-v'; Description = 'Increase verbosity' }
        )
    }
    'set' = @{
        Description = 'Set a configuration setting'
        Options = @(
            @{ Name = '--help'; Short = '-h'; Description = 'Display help' }
            @{ Name = '--verbose'; Short = '-v'; Description = 'Increase verbosity' }
        )
    }
    'shell' = @{
        Description = 'Open a shell in an instance'
        Options = @(
            @{ Name = '--timeout'; Short = '-t'; Description = 'Timeout in seconds' }
            @{ Name = '--help'; Short = '-h'; Description = 'Display help' }
            @{ Name = '--verbose'; Short = '-v'; Description = 'Increase verbosity' }
        )
    }
    'snapshot' = @{
        Description = 'Take a snapshot of an instance'
        Options = @(
            @{ Name = '--name'; Short = '-n'; Description = 'Snapshot name' }
            @{ Name = '--comment'; Short = '-c'; Description = 'Snapshot comment' }
            @{ Name = '--help'; Short = '-h'; Description = 'Display help' }
            @{ Name = '--verbose'; Short = '-v'; Description = 'Increase verbosity' }
        )
    }
    'start' = @{
        Description = 'Start instances'
        Options = @(
            @{ Name = '--all'; Description = 'Start all instances' }
            @{ Name = '--timeout'; Short = '-t'; Description = 'Timeout in seconds' }
            @{ Name = '--help'; Short = '-h'; Description = 'Display help' }
            @{ Name = '--verbose'; Short = '-v'; Description = 'Increase verbosity' }
        )
    }
    'stop' = @{
        Description = 'Stop running instances'
        Options = @(
            @{ Name = '--all'; Description = 'Stop all instances' }
            @{ Name = '--time'; Short = '-t'; Description = 'Delay shutdown by time' }
            @{ Name = '--cancel'; Short = '-c'; Description = 'Cancel scheduled stop' }
            @{ Name = '--force'; Short = '-f'; Description = 'Force stop' }
            @{ Name = '--help'; Short = '-h'; Description = 'Display help' }
            @{ Name = '--verbose'; Short = '-v'; Description = 'Increase verbosity' }
        )
    }
    'suspend' = @{
        Description = 'Suspend running instances'
        Options = @(
            @{ Name = '--all'; Description = 'Suspend all instances' }
            @{ Name = '--help'; Short = '-h'; Description = 'Display help' }
            @{ Name = '--verbose'; Short = '-v'; Description = 'Increase verbosity' }
        )
    }
    'transfer' = @{
        Description = 'Transfer files between host and instance'
        Options = @(
            @{ Name = '--recursive'; Short = '-r'; Description = 'Transfer directories recursively' }
            @{ Name = '--parents'; Short = '-p'; Description = 'Make parent directories as needed' }
            @{ Name = '--help'; Short = '-h'; Description = 'Display help' }
            @{ Name = '--verbose'; Short = '-v'; Description = 'Increase verbosity' }
        )
    }
    'umount' = @{
        Description = 'Unmount a directory from an instance'
        Options = @(
            @{ Name = '--help'; Short = '-h'; Description = 'Display help' }
            @{ Name = '--verbose'; Short = '-v'; Description = 'Increase verbosity' }
        )
    }
    'unalias' = @{
        Description = 'Remove aliases'
        Options = @(
            @{ Name = '--all'; Description = 'Remove all aliases' }
            @{ Name = '--help'; Short = '-h'; Description = 'Display help' }
            @{ Name = '--verbose'; Short = '-v'; Description = 'Increase verbosity' }
        )
    }
    'version' = @{
        Description = 'Show version details'
        Options = @(
            @{ Name = '--format'; Short = '-f'; Description = 'Output format'; Values = @('table', 'json', 'csv', 'yaml') }
            @{ Name = '--help'; Short = '-h'; Description = 'Display help' }
            @{ Name = '--verbose'; Short = '-v'; Description = 'Increase verbosity' }
        )
    }
}

# Command aliases
$script:CommandAliases = @{
    'ls' = 'list'
    'sh' = 'shell'
    'connect' = 'shell'
    'copy-files' = 'transfer'
    'unmount' = 'umount'
}

# Register the argument completer
Register-ArgumentCompleter -Native -CommandName multipass -ScriptBlock {
    param($wordToComplete, $commandAst, $cursorPosition)

    $commandElements = $commandAst.CommandElements
    $command = @()

    # Build the command array
    for ($i = 1; $i -lt $commandElements.Count; $i++) {
        $element = $commandElements[$i]
        if ($element.Extent.EndOffset -gt $cursorPosition) { break }
        $command += $element.ToString()
    }

    $completions = @()

    # Determine what to complete
    if ($command.Count -eq 0) {
        # Complete commands
        foreach ($cmd in $script:MultipassCommands.Keys) {
            if ($cmd -like "$wordToComplete*") {
                $desc = $script:MultipassCommands[$cmd].Description
                $completions += [CompletionResult]::new($cmd, $cmd, 'ParameterValue', $desc)
            }
        }
        # Add command aliases
        foreach ($alias in $script:CommandAliases.Keys) {
            if ($alias -like "$wordToComplete*") {
                $target = $script:CommandAliases[$alias]
                $desc = "$alias (alias for $target)"
                $completions += [CompletionResult]::new($alias, $alias, 'ParameterValue', $desc)
            }
        }
        # Add user-defined aliases
        foreach ($userAlias in (Get-MultipassAliases)) {
            if ($userAlias -like "$wordToComplete*") {
                $completions += [CompletionResult]::new($userAlias, $userAlias, 'ParameterValue', 'User-defined alias')
            }
        }
    }
    else {
        # Get the actual command (resolve aliases)
        $subCommand = $command[0]
        if ($script:CommandAliases.ContainsKey($subCommand)) {
            $subCommand = $script:CommandAliases[$subCommand]
        }

        # Check if completing an option value
        $lastArg = if ($command.Count -gt 1) { $command[-1] } else { "" }

        if ($script:MultipassCommands.ContainsKey($subCommand)) {
            $cmdDef = $script:MultipassCommands[$subCommand]

            # Check if we're completing a value for an option
            $optionNeedingValue = $null
            foreach ($opt in $cmdDef.Options) {
                if ($lastArg -eq $opt.Name -or $lastArg -eq $opt.Short) {
                    if ($opt.Values) {
                        foreach ($val in $opt.Values) {
                            if ($val -like "$wordToComplete*") {
                                $completions += [CompletionResult]::new($val, $val, 'ParameterValue', $val)
                            }
                        }
                        return $completions
                    }
                    $optionNeedingValue = $opt
                    break
                }
            }

            # Complete options
            if ($wordToComplete -like '-*') {
                foreach ($opt in $cmdDef.Options) {
                    if ($opt.Name -like "$wordToComplete*") {
                        $completions += [CompletionResult]::new($opt.Name, $opt.Name, 'ParameterValue', $opt.Description)
                    }
                    if ($opt.Short -and $opt.Short -like "$wordToComplete*") {
                        $completions += [CompletionResult]::new($opt.Short, $opt.Short, 'ParameterValue', $opt.Description)
                    }
                }
            }

            # Complete dynamic values based on command
            switch ($subCommand) {
                'exec' {
                    foreach ($inst in (Get-MultipassInstances -StateFilter 'Running')) {
                        if ($inst -like "$wordToComplete*") {
                            $completions += [CompletionResult]::new($inst, $inst, 'ParameterValue', 'Running instance')
                        }
                    }
                }
                'start' {
                    foreach ($inst in (Get-MultipassInstances -StateFilter 'Stopped')) {
                        if ($inst -like "$wordToComplete*") {
                            $completions += [CompletionResult]::new($inst, $inst, 'ParameterValue', 'Stopped instance')
                        }
                    }
                    foreach ($inst in (Get-MultipassInstances -StateFilter 'Suspended')) {
                        if ($inst -like "$wordToComplete*") {
                            $completions += [CompletionResult]::new($inst, $inst, 'ParameterValue', 'Suspended instance')
                        }
                    }
                }
                'stop' {
                    foreach ($inst in (Get-MultipassInstances -StateFilter 'Running')) {
                        if ($inst -like "$wordToComplete*") {
                            $completions += [CompletionResult]::new($inst, $inst, 'ParameterValue', 'Running instance')
                        }
                    }
                }
                { $_ -in 'shell', 'mount' } {
                    foreach ($state in @('Running', 'Stopped', 'Suspended')) {
                        foreach ($inst in (Get-MultipassInstances -StateFilter $state)) {
                            if ($inst -like "$wordToComplete*") {
                                $completions += [CompletionResult]::new($inst, $inst, 'ParameterValue', "$state instance")
                            }
                        }
                    }
                }
                { $_ -in 'suspend', 'restart' } {
                    foreach ($inst in (Get-MultipassInstances -StateFilter 'Running')) {
                        if ($inst -like "$wordToComplete*") {
                            $completions += [CompletionResult]::new($inst, $inst, 'ParameterValue', 'Running instance')
                        }
                    }
                }
                { $_ -in 'delete', 'info' } {
                    foreach ($inst in (Get-MultipassInstances)) {
                        if ($inst -like "$wordToComplete*") {
                            $completions += [CompletionResult]::new($inst, $inst, 'ParameterValue', 'Instance')
                        }
                    }
                    foreach ($snap in (Get-MultipassSnapshots)) {
                        if ($snap -like "$wordToComplete*") {
                            $completions += [CompletionResult]::new($snap, $snap, 'ParameterValue', 'Snapshot')
                        }
                    }
                }
                'recover' {
                    foreach ($inst in (Get-MultipassInstances -StateFilter 'Deleted')) {
                        if ($inst -like "$wordToComplete*") {
                            $completions += [CompletionResult]::new($inst, $inst, 'ParameterValue', 'Deleted instance')
                        }
                    }
                }
                { $_ -in 'snapshot', 'clone' } {
                    foreach ($inst in (Get-MultipassInstances -StateFilter 'Stopped')) {
                        if ($inst -like "$wordToComplete*") {
                            $completions += [CompletionResult]::new($inst, $inst, 'ParameterValue', 'Stopped instance')
                        }
                    }
                }
                'umount' {
                    foreach ($inst in (Get-MultipassInstances)) {
                        if ($inst -like "$wordToComplete*") {
                            $completions += [CompletionResult]::new($inst, $inst, 'ParameterValue', 'Instance')
                        }
                    }
                }
                { $_ -in 'get', 'set' } {
                    foreach ($key in (Get-MultipassSettingsKeys)) {
                        if ($key -like "$wordToComplete*") {
                            $completions += [CompletionResult]::new($key, $key, 'ParameterValue', 'Setting key')
                        }
                    }
                }
                'unalias' {
                    foreach ($alias in (Get-MultipassAliases)) {
                        if ($alias -like "$wordToComplete*") {
                            $completions += [CompletionResult]::new($alias, $alias, 'ParameterValue', 'Alias')
                        }
                    }
                }
                'prefer' {
                    foreach ($alias in (Get-MultipassAliases)) {
                        if ($alias -like "$wordToComplete*") {
                            $completions += [CompletionResult]::new($alias, $alias, 'ParameterValue', 'Alias context')
                        }
                    }
                }
                'help' {
                    foreach ($cmd in $script:MultipassCommands.Keys) {
                        if ($cmd -like "$wordToComplete*") {
                            $completions += [CompletionResult]::new($cmd, $cmd, 'ParameterValue', 'Command')
                        }
                    }
                }
                'launch' {
                    if ($lastArg -eq '--network') {
                        foreach ($net in (Get-MultipassNetworks)) {
                            if ($net -like "$wordToComplete*") {
                                $completions += [CompletionResult]::new($net, $net, 'ParameterValue', 'Network')
                            }
                        }
                        if ('bridged' -like "$wordToComplete*") {
                            $completions += [CompletionResult]::new('bridged', 'bridged', 'ParameterValue', 'Bridged network')
                        }
                    }
                }
            }
        }
    }

    return $completions
}

# Display usage information when sourced
Write-Host "Multipass PowerShell completions loaded." -ForegroundColor Green
Write-Host "To enable permanently, add the following to your PowerShell profile:" -ForegroundColor Cyan
Write-Host "  . `"<path-to-this-script>/multipass.ps1`"" -ForegroundColor Yellow
