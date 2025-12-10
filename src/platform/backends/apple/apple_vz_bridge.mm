/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <apple/apple_vz_bridge.h>

#include <CoreFoundation/CoreFoundation.h>
#include <Foundation/Foundation.h>
#include <Virtualization/Virtualization.h>

#include <QString>
#include <QUrl>

namespace
{
NSString* nsStringFromQString(const QString& s)
{
    QByteArray utf8 = s.toUtf8();
    return [NSString stringWithUTF8String:utf8.constData()];
}

NSURL* nsURLFromStdFilesystemPath(const std::filesystem::path& p)
{
    std::string utf8 = p.string();
    NSString* nsString = [NSString stringWithUTF8String:utf8.c_str()];
    return [NSURL fileURLWithPath:nsString];
}
} // namespace

namespace multipass::apple
{
CFError init_with_configuration(const multipass::VirtualMachineDescription& desc,
                                VMHandle& out_handle)
{
    @autoreleasepool
    {
        NSError* err = nil;

        VZVirtualMachineConfiguration* config = [[VZVirtualMachineConfiguration alloc] init];

        // CPU & memory
        config.CPUCount = desc.num_cores;
        config.memorySize = desc.mem_size.in_bytes();

        // Storage devices
        NSMutableArray<VZStorageDeviceConfiguration*>* storageDevices = [NSMutableArray array];

        NSString* diskPath = nsStringFromQString(desc.image.image_path);
        NSURL* diskURL = [NSURL fileURLWithPath:diskPath];
        VZDiskImageStorageDeviceAttachment* diskAttachment =
            [[VZDiskImageStorageDeviceAttachment alloc] initWithURL:diskURL readOnly:NO error:&err];
        if (err)
            return CFError((__bridge_retained CFErrorRef)err);

        VZVirtioBlockDeviceConfiguration* disk =
            [[VZVirtioBlockDeviceConfiguration alloc] initWithAttachment:diskAttachment];
        [storageDevices addObject:disk];

        // Cloud-init ISO
        NSString* cloudIsoPath = nsStringFromQString(desc.cloud_init_iso);
        NSURL* cloudIsoURL = [NSURL fileURLWithPath:cloudIsoPath];
        VZDiskImageStorageDeviceAttachment* cloudAttachment =
            [[VZDiskImageStorageDeviceAttachment alloc] initWithURL:cloudIsoURL
                                                           readOnly:YES
                                                              error:&err];
        if (err)
            return CFError((__bridge_retained CFErrorRef)err);

        VZVirtioBlockDeviceConfiguration* cloudIso =
            [[VZVirtioBlockDeviceConfiguration alloc] initWithAttachment:cloudAttachment];
        [storageDevices addObject:cloudIso];

        config.storageDevices = storageDevices;

        // EFI Variable store
        NSString* efivarsFilename = [NSString stringWithFormat:@"vm-efivars"];
        NSString* efivarsPath =
            [NSTemporaryDirectory() stringByAppendingPathComponent:efivarsFilename];

        // EFI bootloader
        NSURL* efivarsURL = [NSURL fileURLWithPath:efivarsPath];
        VZEFIBootLoader* efi = [[VZEFIBootLoader alloc] init];
        efi.variableStore = [[VZEFIVariableStore alloc]
            initCreatingVariableStoreAtURL:efivarsURL
                                   options:VZEFIVariableStoreInitializationOptionAllowOverwrite
                                     error:&err];
        if (err)
            return CFError((__bridge_retained CFErrorRef)err);

        config.bootLoader = efi;

        // Entropy device
        VZVirtioEntropyDeviceConfiguration* entropy =
            [[VZVirtioEntropyDeviceConfiguration alloc] init];
        config.entropyDevices = @[ entropy ];

        // Network device
        VZVirtioNetworkDeviceConfiguration* netDevice =
            [[VZVirtioNetworkDeviceConfiguration alloc] init];

        VZNATNetworkDeviceAttachment* natAttachment = [[VZNATNetworkDeviceAttachment alloc] init];
        netDevice.attachment = natAttachment;

        VZMACAddress* mac = [[VZMACAddress alloc]
            initWithString:[NSString stringWithCString:desc.default_mac_address.c_str()
                                              encoding:NSUTF8StringEncoding]];
        [netDevice setMACAddress:mac];

        config.networkDevices = @[ netDevice ];

        // Memory balloon device
        VZVirtioTraditionalMemoryBalloonDeviceConfiguration* balloon =
            [[VZVirtioTraditionalMemoryBalloonDeviceConfiguration alloc] init];
        config.memoryBalloonDevices = @[ balloon ];

        // Validate configuration
        if (![config validateWithError:&err])
        {
            if (err)
                return CFError((__bridge_retained CFErrorRef)err);
        }

        // Create VM handle
        VZVirtualMachine* virtualMachine = [[VZVirtualMachine alloc] initWithConfiguration:config];

        void* cfRef = (void*)CFBridgingRetain(virtualMachine);
        out_handle = VMHandle(cfRef, [](void* p) { CFRelease(p); });

        return CFError();
    }
}

CFError start_with_completion_handler(VMHandle& vm_handle)
{
    VZVirtualMachine* vm = (__bridge VZVirtualMachine*)vm_handle.get();

    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    __block CFErrorRef err_ref = nullptr;

    [vm startWithCompletionHandler:^(NSError* _Nullable error) {
      if (error)
      {
          err_ref = (__bridge_retained CFErrorRef)error;
      }

      dispatch_semaphore_signal(sema);
    }];

    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);

    return CFError(err_ref);
}

CFError stop_with_completion_handler(VMHandle& vm_handle)
{
    VZVirtualMachine* vm = (__bridge VZVirtualMachine*)vm_handle.get();

    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    __block CFErrorRef err_ref = nullptr;

    [vm stopWithCompletionHandler:^(NSError* _Nullable error) {
      if (error)
      {
          err_ref = (__bridge_retained CFErrorRef)error;
      }

      dispatch_semaphore_signal(sema);
    }];

    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);

    return CFError(err_ref);
}

CFError request_stop_with_error(VMHandle& vm_handle)
{
    VZVirtualMachine* vm = (__bridge VZVirtualMachine*)vm_handle.get();

    NSError* err = nil;
    if (![vm requestStopWithError:&err] && !err)
    {
        err = [NSError errorWithDomain:@"multipass.apple.vzbridge"
                                  code:-1
                              userInfo:@{NSLocalizedDescriptionKey : @"Unknown error"}];
    }

    return err ? CFError((__bridge_retained CFErrorRef)err) : CFError();
}

CFError pause_with_completion_handler(VMHandle& vm_handle)
{
    VZVirtualMachine* vm = (__bridge VZVirtualMachine*)vm_handle.get();

    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    __block CFErrorRef err_ref = nullptr;

    [vm pauseWithCompletionHandler:^(NSError* _Nullable error) {
      if (error)
      {
          err_ref = (__bridge_retained CFErrorRef)error;
      }

      dispatch_semaphore_signal(sema);
    }];

    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);

    return CFError(err_ref);
}

CFError resume_with_completion_handler(VMHandle& vm_handle)
{
    VZVirtualMachine* vm = (__bridge VZVirtualMachine*)vm_handle.get();

    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    __block CFErrorRef err_ref = nullptr;

    [vm resumeWithCompletionHandler:^(NSError* _Nullable error) {
      if (error)
      {
          err_ref = (__bridge_retained CFErrorRef)error;
      }

      dispatch_semaphore_signal(sema);
    }];

    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);

    return CFError(err_ref);
}

AppleVMState get_state(VMHandle& vm_handle)
{
    VZVirtualMachine* vm = (__bridge VZVirtualMachine*)vm_handle.get();

    return AppleVMState([vm state]);
}

bool can_start(VMHandle& vm_handle)
{
    VZVirtualMachine* vm = (__bridge VZVirtualMachine*)vm_handle.get();

    return [vm canStart];
}

bool can_pause(VMHandle& vm_handle)
{
    VZVirtualMachine* vm = (__bridge VZVirtualMachine*)vm_handle.get();

    return [vm canPause];
}

bool can_resume(VMHandle& vm_handle)
{
    VZVirtualMachine* vm = (__bridge VZVirtualMachine*)vm_handle.get();

    return [vm canResume];
}

bool can_stop(VMHandle& vm_handle)
{
    VZVirtualMachine* vm = (__bridge VZVirtualMachine*)vm_handle.get();

    return [vm canStop];
}

bool can_request_stop(VMHandle& vm_handle)
{
    VZVirtualMachine* vm = (__bridge VZVirtualMachine*)vm_handle.get();

    return [vm canRequestStop];
}
} // namespace multipass::apple
