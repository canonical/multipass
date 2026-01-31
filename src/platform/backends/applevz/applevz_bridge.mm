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

#include <applevz/applevz_bridge.h>

#include <CoreFoundation/CoreFoundation.h>
#include <Foundation/Foundation.h>
#include <Virtualization/Virtualization.h>

#import <objc/objc.h>
#import <objc/runtime.h>

#include <QString>
#include <QUrl>

namespace multipass::applevz
{
struct VirtualMachineHandle
{
    std::shared_ptr<void> vm; // Ownership transfer of VZVirtualMachine*
    dispatch_queue_t queue;   // Dispatch queue for VM operations
    uint64_t id;              // Unique VM ID
};
} // namespace multipass::applevz

namespace
{
// TODO: Replace with unique VM ID once implemented.
static std::atomic<uint64_t> vmIDCounter{0};

NSString* nsstring_from_qstring(const QString& s)
{
    QByteArray utf8 = s.toUtf8();
    return [NSString stringWithUTF8String:utf8.constData()];
}

template <typename BlockType>
multipass::applevz::CFError call_on_vm_queue(const multipass::applevz::VMHandle& vm_handle,
                                             BlockType block,
                                             uint64_t timeout_secs = 120)
{
    __block CFErrorRef err_ref = nullptr;

    // TODO: Remove use of semaphore once Multipass supports async vm operations.
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    dispatch_async(vm_handle->queue, ^{
      block(^(NSError* _Nullable error) {
        if (error)
            err_ref = (__bridge_retained CFErrorRef)error;

        dispatch_semaphore_signal(sema);
      });
    });

    dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, timeout_secs * NSEC_PER_SEC);
    if (dispatch_semaphore_wait(sema, timeout))
    {
        NSError* err =
            [NSError errorWithDomain:@"multipass.apple.vzbridge"
                                code:-1
                            userInfo:@{NSLocalizedDescriptionKey : @"VM operation timed out"}];
        err_ref = (__bridge_retained CFErrorRef)err;
    }

    return multipass::applevz::CFError(err_ref);
}

template <typename ResultType, typename BlockType>
ResultType query_on_vm_queue(const multipass::applevz::VMHandle& vm_handle, BlockType block)
{
    __block ResultType result{};
    dispatch_sync(vm_handle->queue, ^{
      result = block();
    });
    return result;
}
} // namespace

namespace multipass::applevz
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

        // cachingMode is set to VZDiskImageCachingModeCached so as to avoid disk corruption on ARM:
        // - https://github.com/utmapp/UTM/issues/4840#issuecomment-1824340975
        // - https://github.com/utmapp/UTM/issues/4840#issuecomment-1824542732
        NSString* diskPath = nsstring_from_qstring(desc.image.image_path);
        NSURL* diskURL = [NSURL fileURLWithPath:diskPath];
        VZDiskImageStorageDeviceAttachment* diskAttachment =
            [[VZDiskImageStorageDeviceAttachment alloc]
                        initWithURL:diskURL
                           readOnly:NO
                        cachingMode:VZDiskImageCachingModeCached
                synchronizationMode:VZDiskImageSynchronizationModeFsync
                              error:&err];
        if (err)
            return CFError((__bridge_retained CFErrorRef)err);

        VZVirtioBlockDeviceConfiguration* disk =
            [[VZVirtioBlockDeviceConfiguration alloc] initWithAttachment:diskAttachment];
        [storageDevices addObject:disk];

        // Cloud-init ISO
        NSString* cloudIsoPath = nsstring_from_qstring(desc.cloud_init_iso);
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

        out_handle = std::make_shared<VirtualMachineHandle>();

        out_handle->id = vmIDCounter.fetch_add(1, std::memory_order_relaxed);

        // Create dispatch queue
        dispatch_queue_t queue = dispatch_queue_create(
            fmt::format("com.canonical.multipass.vm.queue.{}", out_handle->id).c_str(),
            DISPATCH_QUEUE_SERIAL);

        // Create VM handle
        VZVirtualMachine* virtualMachine = [[VZVirtualMachine alloc] initWithConfiguration:config
                                                                                     queue:queue];

        out_handle->queue = queue;
        out_handle->vm = std::shared_ptr<void>((void*)CFBridgingRetain(virtualMachine),
                                               [](void* p) { CFRelease(p); });

        return CFError();
    }
}

CFError start_with_completion_handler(const VMHandle& vm_handle)
{
    VZVirtualMachine* vm = (__bridge VZVirtualMachine*)vm_handle->vm.get();

    return call_on_vm_queue(vm_handle,
                            [&](auto completion) { [vm startWithCompletionHandler:completion]; });
}

CFError stop_with_completion_handler(const VMHandle& vm_handle)
{
    VZVirtualMachine* vm = (__bridge VZVirtualMachine*)vm_handle->vm.get();

    return call_on_vm_queue(vm_handle,
                            [&](auto completion) { [vm stopWithCompletionHandler:completion]; });
}

CFError request_stop_with_error(const VMHandle& vm_handle)
{
    VZVirtualMachine* vm = (__bridge VZVirtualMachine*)vm_handle->vm.get();

    return call_on_vm_queue(vm_handle, [&](auto completion) {
        NSError* err = nil;
        BOOL ok = [vm requestStopWithError:&err];
        if (!ok && !err)
        {
            err = [NSError errorWithDomain:@"multipass.applevz.bridge"
                                      code:-1
                                  userInfo:@{NSLocalizedDescriptionKey : @"Unknown error"}];
        }
        completion(err);
    });
}

CFError pause_with_completion_handler(const VMHandle& vm_handle)
{
    VZVirtualMachine* vm = (__bridge VZVirtualMachine*)vm_handle->vm.get();

    return call_on_vm_queue(vm_handle,
                            [&](auto completion) { [vm pauseWithCompletionHandler:completion]; });
}

CFError resume_with_completion_handler(const VMHandle& vm_handle)
{
    VZVirtualMachine* vm = (__bridge VZVirtualMachine*)vm_handle->vm.get();

    return call_on_vm_queue(vm_handle,
                            [&](auto completion) { [vm resumeWithCompletionHandler:completion]; });
}

AppleVMState get_state(const VMHandle& vm_handle)
{
    VZVirtualMachine* vm = (__bridge VZVirtualMachine*)vm_handle->vm.get();
    return AppleVMState(
        query_on_vm_queue<VZVirtualMachineState>(vm_handle, [&]() { return [vm state]; }));
}

bool can_start(const VMHandle& vm_handle)
{
    VZVirtualMachine* vm = (__bridge VZVirtualMachine*)vm_handle->vm.get();
    return query_on_vm_queue<bool>(vm_handle, [&]() { return [vm canStart]; });
}

bool can_pause(const VMHandle& vm_handle)
{
    VZVirtualMachine* vm = (__bridge VZVirtualMachine*)vm_handle->vm.get();
    return query_on_vm_queue<bool>(vm_handle, [&]() { return [vm canPause]; });
}

bool can_resume(const VMHandle& vm_handle)
{
    VZVirtualMachine* vm = (__bridge VZVirtualMachine*)vm_handle->vm.get();
    return query_on_vm_queue<bool>(vm_handle, [&]() { return [vm canResume]; });
}

bool can_stop(const VMHandle& vm_handle)
{
    VZVirtualMachine* vm = (__bridge VZVirtualMachine*)vm_handle->vm.get();
    return query_on_vm_queue<bool>(vm_handle, [&]() { return [vm canStop]; });
}

bool can_request_stop(const VMHandle& vm_handle)
{
    VZVirtualMachine* vm = (__bridge VZVirtualMachine*)vm_handle->vm.get();
    return query_on_vm_queue<bool>(vm_handle, [&]() { return [vm canRequestStop]; });
}

bool is_supported()
{
    return [VZVirtualMachine isSupported];
}
} // namespace multipass::applevz
