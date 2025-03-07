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

#include "hyperv_api/hcs/hyperv_hcs_add_endpoint_params.h"
#include "hyperv_api/hcs/hyperv_hcs_api_wrapper.h"
#include "hyperv_api/hcs/hyperv_hcs_create_compute_system_params.h"
#include "hyperv_test_utils.h"
#include "tests/mock_logger.h"
#include "gmock/gmock.h"

#include <ComputeDefs.h>
#include <multipass/logging/level.h>
#include <winbase.h>
#include <winerror.h>
#include <winnt.h>

namespace mpt = multipass::test;
namespace mpl = multipass::logging;

using testing::DoAll;
using testing::Return;

namespace multipass::test
{
using uut_t = hyperv::hcs::HCSWrapper;

struct HyperVHCSAPI_UnitTests : public ::testing::Test
{
    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();

    void SetUp() override
    {

        // Each of the unit tests are expected to have their own mock functions
        // and override the mock_api_table with them. Hence, the stub mocks should
        // not be called at all.
        // If any of them do get called, then:
        //
        // a-) You have forgotten to mock something
        // b-) The implementation is using a function that you didn't expect
        //
        // Either way, you should have a look.
        EXPECT_NO_CALL(stub_mock_create_operation);
        EXPECT_NO_CALL(stub_mock_wait_for_operation_result);
        EXPECT_NO_CALL(stub_mock_close_operation);
        EXPECT_NO_CALL(stub_mock_create_compute_system);
        EXPECT_NO_CALL(stub_mock_open_compute_system);
        EXPECT_NO_CALL(stub_mock_start_compute_system);
        EXPECT_NO_CALL(stub_mock_shutdown_compute_system);
        EXPECT_NO_CALL(stub_mock_terminate_compute_system);
        EXPECT_NO_CALL(stub_mock_close_compute_system);
        EXPECT_NO_CALL(stub_mock_pause_compute_system);
        EXPECT_NO_CALL(stub_mock_resume_compute_system);
        EXPECT_NO_CALL(stub_mock_modify_compute_system);
        EXPECT_NO_CALL(stub_mock_get_compute_system_properties);
        EXPECT_NO_CALL(stub_mock_grant_vm_access);
        EXPECT_NO_CALL(stub_mock_revoke_vm_access);
        EXPECT_NO_CALL(stub_mock_enumerate_compute_systems);
        EXPECT_NO_CALL(stub_mock_local_free);
    }

    void TearDown() override
    {
    }

    // Set of placeholder mocks in order to catch *unexpected* calls.
    ::testing::MockFunction<decltype(HcsCreateOperation)> stub_mock_create_operation;
    ::testing::MockFunction<decltype(HcsWaitForOperationResult)> stub_mock_wait_for_operation_result;
    ::testing::MockFunction<decltype(HcsCloseOperation)> stub_mock_close_operation;
    ::testing::MockFunction<decltype(HcsCreateComputeSystem)> stub_mock_create_compute_system;
    ::testing::MockFunction<decltype(HcsOpenComputeSystem)> stub_mock_open_compute_system;
    ::testing::MockFunction<decltype(HcsStartComputeSystem)> stub_mock_start_compute_system;
    ::testing::MockFunction<decltype(HcsShutDownComputeSystem)> stub_mock_shutdown_compute_system;
    ::testing::MockFunction<decltype(HcsTerminateComputeSystem)> stub_mock_terminate_compute_system;
    ::testing::MockFunction<decltype(HcsCloseComputeSystem)> stub_mock_close_compute_system;
    ::testing::MockFunction<decltype(HcsPauseComputeSystem)> stub_mock_pause_compute_system;
    ::testing::MockFunction<decltype(HcsResumeComputeSystem)> stub_mock_resume_compute_system;
    ::testing::MockFunction<decltype(HcsModifyComputeSystem)> stub_mock_modify_compute_system;
    ::testing::MockFunction<decltype(HcsGetComputeSystemProperties)> stub_mock_get_compute_system_properties;
    ::testing::MockFunction<decltype(HcsGrantVmAccess)> stub_mock_grant_vm_access;
    ::testing::MockFunction<decltype(HcsRevokeVmAccess)> stub_mock_revoke_vm_access;
    ::testing::MockFunction<decltype(HcsEnumerateComputeSystems)> stub_mock_enumerate_compute_systems;
    ::testing::MockFunction<decltype(::LocalFree)> stub_mock_local_free;

    // Initialize the API table with stub functions, so if any of these fire without
    // our will, we'll know.
    hyperv::hcs::HCSAPITable mock_api_table{stub_mock_create_operation.AsStdFunction(),
                                            stub_mock_wait_for_operation_result.AsStdFunction(),
                                            stub_mock_close_operation.AsStdFunction(),
                                            stub_mock_create_compute_system.AsStdFunction(),
                                            stub_mock_open_compute_system.AsStdFunction(),
                                            stub_mock_start_compute_system.AsStdFunction(),
                                            stub_mock_shutdown_compute_system.AsStdFunction(),
                                            stub_mock_terminate_compute_system.AsStdFunction(),
                                            stub_mock_close_compute_system.AsStdFunction(),
                                            stub_mock_pause_compute_system.AsStdFunction(),
                                            stub_mock_resume_compute_system.AsStdFunction(),
                                            stub_mock_modify_compute_system.AsStdFunction(),
                                            stub_mock_get_compute_system_properties.AsStdFunction(),
                                            stub_mock_grant_vm_access.AsStdFunction(),
                                            stub_mock_revoke_vm_access.AsStdFunction(),
                                            stub_mock_enumerate_compute_systems.AsStdFunction(),
                                            stub_mock_local_free.AsStdFunction()};

    // Sentinel values as mock API parameters. These handles are opaque handles and
    // they're not being dereferenced in any way -- only address values are compared.
    inline static auto mock_operation_object = reinterpret_cast<HCS_OPERATION>(0xbadf00d);
    inline static auto mock_compute_system_object = reinterpret_cast<HCS_SYSTEM>(0xbadcafe);

    // Generic error message for all tests, intended to be used for API calls returning
    // an "error_record".
    inline static wchar_t mock_error_msg[16] = L"It's a failure.";
    inline static wchar_t mock_success_msg[16] = L"Succeeded.";
    inline static wchar_t operation_fail_msg[22] = L"HCS operation failed!";
    inline static wchar_t hcs_create_operation_fail_msg[27] = L"HcsCreateOperation failed!";
    inline static wchar_t hcs_open_compute_system_fail_msg[29] = L"HcsOpenComputeSystem failed!";

    template <typename TargetFnSig, typename UutCallableT, typename MockCallableT, typename ApiFnT>
    void generic_operation_happy_path(ApiFnT& target_api_function,
                                      UutCallableT uut_callback,
                                      MockCallableT mock_callback,
                                      PWSTR operation_result_document = nullptr,
                                      PWSTR expected_status_msg = nullptr);

    template <typename TargetFnSig, typename UutCallableT, typename MockCallableT, typename ApiFnT>
    void generic_operation_fail(ApiFnT& target_api_function,
                                UutCallableT uut_callback,
                                MockCallableT mock_callback,
                                PWSTR expected_status_msg = operation_fail_msg);

    template <typename TargetFnSig, typename UutCallableT, typename MockCallableT, typename ApiFnT>
    void generic_operation_wait_for_operation_fail(ApiFnT& target_api_function,
                                                   UutCallableT uut_callback,
                                                   MockCallableT mock_callback,
                                                   PWSTR operation_result_document = mock_error_msg,
                                                   PWSTR expected_status_msg = mock_error_msg);

    template <typename TargetFnSig, typename UutCallableT, typename ApiFnT>
    void generic_operation_hcs_open_fail(ApiFnT& target_api_function,
                                         UutCallableT uut_callback,
                                         PWSTR expected_status_msg = hcs_open_compute_system_fail_msg);

    template <typename TargetFnSig, typename UutCallableT, typename ApiFnT>
    void generic_operation_create_operation_fail(ApiFnT& target_api_function,
                                                 UutCallableT uut_callback,
                                                 PWSTR expected_status_msg = hcs_create_operation_fail_msg);
};

// ---------------------------------------------------------

/**
 * Success scenario: Everything goes as expected.
 */
TEST_F(HyperVHCSAPI_UnitTests, create_compute_system_happy_path)
{
    /******************************************************
     * Override the default mock functions.
     ******************************************************/
    ::testing::MockFunction<decltype(HcsCreateOperation)> mock_create_operation;
    ::testing::MockFunction<decltype(HcsCloseOperation)> mock_close_operation;
    ::testing::MockFunction<decltype(HcsWaitForOperationResult)> mock_wait_for_operation_result;
    ::testing::MockFunction<decltype(HcsCreateComputeSystem)> mock_create_compute_system;
    ::testing::MockFunction<decltype(HcsCloseComputeSystem)> mock_close_compute_system;
    ::testing::MockFunction<decltype(::LocalFree)> mock_local_free;

    mock_api_table.CreateOperation = mock_create_operation.AsStdFunction();
    mock_api_table.CloseOperation = mock_close_operation.AsStdFunction();
    mock_api_table.WaitForOperationResult = mock_wait_for_operation_result.AsStdFunction();
    mock_api_table.CreateComputeSystem = mock_create_compute_system.AsStdFunction();
    mock_api_table.CloseComputeSystem = mock_close_compute_system.AsStdFunction();
    mock_api_table.LocalFree = mock_local_free.AsStdFunction();

    constexpr auto expected_vm_settings_json = LR"(
    {
        "SchemaVersion": {
            "Major": 2,
            "Minor": 1
        },
        "Owner": "Multipass",
        "ShouldTerminateOnLastHandleClosed": false,
        "VirtualMachine": {
            "Chipset": {
                "Uefi": {
                    "BootThis": {
                        "DevicePath": "Primary disk",
                        "DiskNumber": 0,
                        "DeviceType": "ScsiDrive"
                    },
                    "Console": "ComPort1"
                }
            },
            "ComputeTopology": {
                "Memory": {
                    "Backing": "Virtual",
                    "SizeInMB": 16384
                },
                "Processor": {
                    "Count": 8
                }
            },
            "Devices": {
                "ComPorts": {
                    "0": {
                        "NamedPipe": "\\\\.\\pipe\\test_vm"
                    }
                },
                "Scsi": {
                    "cloud-init iso file": {
                        "Attachments": {
                            "0": {
                                "Type": "Iso",
                                "Path": "cloudinit iso path",
                                "ReadOnly": true
                            }
                        }
                    },
                    "Primary disk": {
                        "Attachments": {
                            "0": {
                                "Type": "VirtualDisk",
                                "Path": "virtual disk path",
                                "ReadOnly": false
                            }
                        }
                    },
                }
            }
        }
    })";

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_create_operation, Call)
            .WillOnce(DoAll(
                [](const void* context, HCS_OPERATION_COMPLETION callback) {
                    ASSERT_EQ(nullptr, context);
                    ASSERT_EQ(nullptr, callback);
                },
                Return(mock_operation_object)));

        EXPECT_CALL(mock_close_operation, Call).WillOnce([](HCS_OPERATION op) {
            ASSERT_EQ(op, mock_operation_object);
        });

        EXPECT_CALL(mock_wait_for_operation_result, Call)
            .WillOnce(DoAll(
                [](HCS_OPERATION operation, DWORD timeoutMs, PWSTR* resultDocument) {
                    ASSERT_EQ(operation, mock_operation_object);
                    ASSERT_EQ(timeoutMs, 240000);
                    ASSERT_NE(nullptr, resultDocument);
                    ASSERT_EQ(nullptr, *resultDocument);
                    *resultDocument = mock_success_msg;
                },
                Return(NOERROR)));

        EXPECT_CALL(mock_create_compute_system, Call)
            .WillOnce(DoAll(
                [](PCWSTR id,
                   PCWSTR configuration,
                   HCS_OPERATION operation,
                   const SECURITY_DESCRIPTOR* securityDescriptor,
                   HCS_SYSTEM* computeSystem) {
                    ASSERT_STREQ(L"test_vm", id);

                    const auto config_no_whitespace = trim_whitespace(configuration);
                    const auto expected_no_whitespace = trim_whitespace(expected_vm_settings_json);

                    ASSERT_STREQ(expected_no_whitespace.c_str(), config_no_whitespace.c_str());
                    ASSERT_EQ(mock_operation_object, operation);
                    ASSERT_EQ(nullptr, securityDescriptor);
                    ASSERT_NE(nullptr, computeSystem);
                    ASSERT_EQ(nullptr, *computeSystem);
                    *computeSystem = mock_compute_system_object;
                },
                Return(NOERROR)));

        EXPECT_CALL(mock_close_compute_system, Call).WillOnce([](HCS_SYSTEM computeSystem) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
        });

        EXPECT_CALL(mock_local_free, Call)
            .WillOnce(DoAll([](HLOCAL ptr) { ASSERT_EQ(ptr, mock_success_msg); }, Return(nullptr)));

        logger_scope.mock_logger->expect_log(mpl::Level::debug, "HCSWrapper::HCSWrapper(...)");
        logger_scope.mock_logger->expect_log(
            mpl::Level::debug,
            "HCSWrapper::create_compute_system(...) > params: Compute System name: (test_vm) | vCPU count: (8) | "
            "Memory size: (16384 MiB) | cloud-init ISO path: (cloudinit iso path) | VHDX path: (virtual disk path)");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "create_operation(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "wait_for_operation_result(...)");
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        uut_t uut{mock_api_table};
        hyperv::hcs::CreateComputeSystemParameters params{};
        params.name = "test_vm";
        params.cloudinit_iso_path = "cloudinit iso path";
        params.vhdx_path = "virtual disk path";
        params.memory_size_mb = 16384;
        params.processor_count = 8;

        const auto& [status, status_msg] = uut.create_compute_system(params);
        ASSERT_TRUE(status);
        ASSERT_FALSE(status_msg.empty());
        ASSERT_STREQ(status_msg.c_str(), mock_success_msg);
    }
}

// ---------------------------------------------------------

/**
 * Success scenario: Everything goes as expected.
 */
TEST_F(HyperVHCSAPI_UnitTests, create_compute_system_wo_cloudinit)
{
    /******************************************************
     * Override the default mock functions.
     ******************************************************/
    ::testing::MockFunction<decltype(HcsCreateOperation)> mock_create_operation;
    ::testing::MockFunction<decltype(HcsCloseOperation)> mock_close_operation;
    ::testing::MockFunction<decltype(HcsWaitForOperationResult)> mock_wait_for_operation_result;
    ::testing::MockFunction<decltype(HcsCreateComputeSystem)> mock_create_compute_system;
    ::testing::MockFunction<decltype(HcsCloseComputeSystem)> mock_close_compute_system;
    ::testing::MockFunction<decltype(::LocalFree)> mock_local_free;

    mock_api_table.CreateOperation = mock_create_operation.AsStdFunction();
    mock_api_table.CloseOperation = mock_close_operation.AsStdFunction();
    mock_api_table.WaitForOperationResult = mock_wait_for_operation_result.AsStdFunction();
    mock_api_table.CreateComputeSystem = mock_create_compute_system.AsStdFunction();
    mock_api_table.CloseComputeSystem = mock_close_compute_system.AsStdFunction();
    mock_api_table.LocalFree = mock_local_free.AsStdFunction();

    constexpr auto expected_vm_settings_json = LR"(
    {
        "SchemaVersion": {
            "Major": 2,
            "Minor": 1
        },
        "Owner": "Multipass",
        "ShouldTerminateOnLastHandleClosed": false,
        "VirtualMachine": {
            "Chipset": {
                "Uefi": {
                    "BootThis": {
                        "DevicePath": "Primary disk",
                        "DiskNumber": 0,
                        "DeviceType": "ScsiDrive"
                    },
                    "Console": "ComPort1"
                }
            },
            "ComputeTopology": {
                "Memory": {
                    "Backing": "Virtual",
                    "SizeInMB": 16384
                },
                "Processor": {
                    "Count": 8
                }
            },
            "Devices": {
                "ComPorts": {
                    "0": {
                        "NamedPipe": "\\\\.\\pipe\\test_vm"
                    }
                },
                "Scsi": {
                    "Primary disk": {
                        "Attachments": {
                            "0": {
                                "Type": "VirtualDisk",
                                "Path": "virtual disk path",
                                "ReadOnly": false
                            }
                        }
                    },
                }
            }
        }
    })";

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_create_operation, Call)
            .WillOnce(DoAll(
                [](const void* context, HCS_OPERATION_COMPLETION callback) {
                    ASSERT_EQ(nullptr, context);
                    ASSERT_EQ(nullptr, callback);
                },
                Return(mock_operation_object)));

        EXPECT_CALL(mock_close_operation, Call).WillOnce([](HCS_OPERATION op) {
            ASSERT_EQ(op, mock_operation_object);
        });

        EXPECT_CALL(mock_wait_for_operation_result, Call)
            .WillOnce(DoAll(
                [](HCS_OPERATION operation, DWORD timeoutMs, PWSTR* resultDocument) {
                    ASSERT_EQ(operation, mock_operation_object);
                    ASSERT_EQ(timeoutMs, 240000);
                    ASSERT_NE(nullptr, resultDocument);
                    ASSERT_EQ(nullptr, *resultDocument);
                    *resultDocument = mock_success_msg;
                },
                Return(NOERROR)));

        EXPECT_CALL(mock_create_compute_system, Call)
            .WillOnce(DoAll(
                [](PCWSTR id,
                   PCWSTR configuration,
                   HCS_OPERATION operation,
                   const SECURITY_DESCRIPTOR* securityDescriptor,
                   HCS_SYSTEM* computeSystem) {
                    ASSERT_STREQ(L"test_vm", id);

                    const auto config_no_whitespace = trim_whitespace(configuration);
                    const auto expected_no_whitespace = trim_whitespace(expected_vm_settings_json);

                    ASSERT_STREQ(expected_no_whitespace.c_str(), config_no_whitespace.c_str());
                    ASSERT_EQ(mock_operation_object, operation);
                    ASSERT_EQ(nullptr, securityDescriptor);
                    ASSERT_NE(nullptr, computeSystem);
                    ASSERT_EQ(nullptr, *computeSystem);
                    *computeSystem = mock_compute_system_object;
                },
                Return(NOERROR)));

        EXPECT_CALL(mock_close_compute_system, Call).WillOnce([](HCS_SYSTEM computeSystem) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
        });

        EXPECT_CALL(mock_local_free, Call)
            .WillOnce(DoAll([](HLOCAL ptr) { ASSERT_EQ(ptr, mock_success_msg); }, Return(nullptr)));

        logger_scope.mock_logger->expect_log(mpl::Level::debug, "HCSWrapper::HCSWrapper(...)");
        logger_scope.mock_logger->expect_log(
            mpl::Level::debug,
            "HCSWrapper::create_compute_system(...) > params: Compute System name: (test_vm) | vCPU count: (8) | "
            "Memory size: (16384 MiB) | cloud-init ISO path: () | VHDX path: (virtual disk path)");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "create_operation(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "wait_for_operation_result(...)");
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        uut_t uut{mock_api_table};
        hyperv::hcs::CreateComputeSystemParameters params{};
        params.name = "test_vm";
        params.cloudinit_iso_path = "";
        params.vhdx_path = "virtual disk path";
        params.memory_size_mb = 16384;
        params.processor_count = 8;

        const auto& [status, status_msg] = uut.create_compute_system(params);
        ASSERT_TRUE(status);
        ASSERT_FALSE(status_msg.empty());
        ASSERT_STREQ(status_msg.c_str(), mock_success_msg);
    }
}

// ---------------------------------------------------------

/**
 * Success scenario: Everything goes as expected.
 */
TEST_F(HyperVHCSAPI_UnitTests, create_compute_system_wo_vhdx)
{
    /******************************************************
     * Override the default mock functions.
     ******************************************************/
    ::testing::MockFunction<decltype(HcsCreateOperation)> mock_create_operation;
    ::testing::MockFunction<decltype(HcsCloseOperation)> mock_close_operation;
    ::testing::MockFunction<decltype(HcsWaitForOperationResult)> mock_wait_for_operation_result;
    ::testing::MockFunction<decltype(HcsCreateComputeSystem)> mock_create_compute_system;
    ::testing::MockFunction<decltype(HcsCloseComputeSystem)> mock_close_compute_system;
    ::testing::MockFunction<decltype(::LocalFree)> mock_local_free;

    mock_api_table.CreateOperation = mock_create_operation.AsStdFunction();
    mock_api_table.CloseOperation = mock_close_operation.AsStdFunction();
    mock_api_table.WaitForOperationResult = mock_wait_for_operation_result.AsStdFunction();
    mock_api_table.CreateComputeSystem = mock_create_compute_system.AsStdFunction();
    mock_api_table.CloseComputeSystem = mock_close_compute_system.AsStdFunction();
    mock_api_table.LocalFree = mock_local_free.AsStdFunction();

    constexpr auto expected_vm_settings_json = LR"(
    {
        "SchemaVersion": {
            "Major": 2,
            "Minor": 1
        },
        "Owner": "Multipass",
        "ShouldTerminateOnLastHandleClosed": false,
        "VirtualMachine": {
            "Chipset": {
                "Uefi": {
                    "BootThis": {
                        "DevicePath": "Primary disk",
                        "DiskNumber": 0,
                        "DeviceType": "ScsiDrive"
                    },
                    "Console": "ComPort1"
                }
            },
            "ComputeTopology": {
                "Memory": {
                    "Backing": "Virtual",
                    "SizeInMB": 16384
                },
                "Processor": {
                    "Count": 8
                }
            },
            "Devices": {
                "ComPorts": {
                    "0": {
                        "NamedPipe": "\\\\.\\pipe\\test_vm"
                    }
                },
                "Scsi": {
                    "cloud-init iso file": {
                        "Attachments": {
                            "0": {
                                "Type": "Iso",
                                "Path": "cloudinit iso path",
                                "ReadOnly": true
                            }
                        }
                    },
                }
            }
        }
    })";

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_create_operation, Call)
            .WillOnce(DoAll(
                [](const void* context, HCS_OPERATION_COMPLETION callback) {
                    ASSERT_EQ(nullptr, context);
                    ASSERT_EQ(nullptr, callback);
                },
                Return(mock_operation_object)));

        EXPECT_CALL(mock_close_operation, Call).WillOnce([](HCS_OPERATION op) {
            ASSERT_EQ(op, mock_operation_object);
        });

        EXPECT_CALL(mock_wait_for_operation_result, Call)
            .WillOnce(DoAll(
                [](HCS_OPERATION operation, DWORD timeoutMs, PWSTR* resultDocument) {
                    ASSERT_EQ(operation, mock_operation_object);
                    ASSERT_EQ(timeoutMs, 240000);
                    ASSERT_NE(nullptr, resultDocument);
                    ASSERT_EQ(nullptr, *resultDocument);
                    *resultDocument = mock_success_msg;
                },
                Return(NOERROR)));

        EXPECT_CALL(mock_create_compute_system, Call)
            .WillOnce(DoAll(
                [](PCWSTR id,
                   PCWSTR configuration,
                   HCS_OPERATION operation,
                   const SECURITY_DESCRIPTOR* securityDescriptor,
                   HCS_SYSTEM* computeSystem) {
                    ASSERT_STREQ(L"test_vm", id);

                    const auto config_no_whitespace = trim_whitespace(configuration);
                    const auto expected_no_whitespace = trim_whitespace(expected_vm_settings_json);

                    ASSERT_STREQ(expected_no_whitespace.c_str(), config_no_whitespace.c_str());
                    ASSERT_EQ(mock_operation_object, operation);
                    ASSERT_EQ(nullptr, securityDescriptor);
                    ASSERT_NE(nullptr, computeSystem);
                    ASSERT_EQ(nullptr, *computeSystem);
                    *computeSystem = mock_compute_system_object;
                },
                Return(NOERROR)));

        EXPECT_CALL(mock_close_compute_system, Call).WillOnce([](HCS_SYSTEM computeSystem) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
        });

        EXPECT_CALL(mock_local_free, Call)
            .WillOnce(DoAll([](HLOCAL ptr) { ASSERT_EQ(ptr, mock_success_msg); }, Return(nullptr)));

        logger_scope.mock_logger->expect_log(mpl::Level::debug, "HCSWrapper::HCSWrapper(...)");
        logger_scope.mock_logger->expect_log(
            mpl::Level::debug,
            "HCSWrapper::create_compute_system(...) > params: Compute System name: (test_vm) | vCPU count: (8) | "
            "Memory size: (16384 MiB) | cloud-init ISO path: (cloudinit iso path) | VHDX path: ()");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "create_operation(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "wait_for_operation_result(...)");
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        uut_t uut{mock_api_table};
        hyperv::hcs::CreateComputeSystemParameters params{};
        params.name = "test_vm";
        params.cloudinit_iso_path = "cloudinit iso path";
        params.vhdx_path = "";
        params.memory_size_mb = 16384;
        params.processor_count = 8;

        const auto& [status, status_msg] = uut.create_compute_system(params);
        ASSERT_TRUE(status);
        ASSERT_FALSE(status_msg.empty());
        ASSERT_STREQ(status_msg.c_str(), mock_success_msg);
    }
}

// ---------------------------------------------------------

/**
 * Success scenario: Everything goes as expected.
 */
TEST_F(HyperVHCSAPI_UnitTests, create_compute_system_wo_cloudinit_and_vhdx)
{
    /******************************************************
     * Override the default mock functions.
     ******************************************************/
    ::testing::MockFunction<decltype(HcsCreateOperation)> mock_create_operation;
    ::testing::MockFunction<decltype(HcsCloseOperation)> mock_close_operation;
    ::testing::MockFunction<decltype(HcsWaitForOperationResult)> mock_wait_for_operation_result;
    ::testing::MockFunction<decltype(HcsCreateComputeSystem)> mock_create_compute_system;
    ::testing::MockFunction<decltype(HcsCloseComputeSystem)> mock_close_compute_system;
    ::testing::MockFunction<decltype(::LocalFree)> mock_local_free;

    mock_api_table.CreateOperation = mock_create_operation.AsStdFunction();
    mock_api_table.CloseOperation = mock_close_operation.AsStdFunction();
    mock_api_table.WaitForOperationResult = mock_wait_for_operation_result.AsStdFunction();
    mock_api_table.CreateComputeSystem = mock_create_compute_system.AsStdFunction();
    mock_api_table.CloseComputeSystem = mock_close_compute_system.AsStdFunction();
    mock_api_table.LocalFree = mock_local_free.AsStdFunction();

    constexpr auto expected_vm_settings_json = LR"(
    {
        "SchemaVersion": {
            "Major": 2,
            "Minor": 1
        },
        "Owner": "Multipass",
        "ShouldTerminateOnLastHandleClosed": false,
        "VirtualMachine": {
            "Chipset": {
                "Uefi": {
                    "BootThis": {
                        "DevicePath": "Primary disk",
                        "DiskNumber": 0,
                        "DeviceType": "ScsiDrive"
                    },
                    "Console": "ComPort1"
                }
            },
            "ComputeTopology": {
                "Memory": {
                    "Backing": "Virtual",
                    "SizeInMB": 16384
                },
                "Processor": {
                    "Count": 8
                }
            },
            "Devices": {
                "ComPorts": {
                    "0": {
                        "NamedPipe": "\\\\.\\pipe\\test_vm"
                    }
                },
                "Scsi": {
                }
            }
        }
    })";

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_create_operation, Call)
            .WillOnce(DoAll(
                [](const void* context, HCS_OPERATION_COMPLETION callback) {
                    ASSERT_EQ(nullptr, context);
                    ASSERT_EQ(nullptr, callback);
                },
                Return(mock_operation_object)));

        EXPECT_CALL(mock_close_operation, Call).WillOnce([](HCS_OPERATION op) {
            ASSERT_EQ(op, mock_operation_object);
        });

        EXPECT_CALL(mock_wait_for_operation_result, Call)
            .WillOnce(DoAll(
                [](HCS_OPERATION operation, DWORD timeoutMs, PWSTR* resultDocument) {
                    ASSERT_EQ(operation, mock_operation_object);
                    ASSERT_EQ(timeoutMs, 240000);
                    ASSERT_NE(nullptr, resultDocument);
                    ASSERT_EQ(nullptr, *resultDocument);
                    *resultDocument = mock_success_msg;
                },
                Return(NOERROR)));

        EXPECT_CALL(mock_create_compute_system, Call)
            .WillOnce(DoAll(
                [](PCWSTR id,
                   PCWSTR configuration,
                   HCS_OPERATION operation,
                   const SECURITY_DESCRIPTOR* securityDescriptor,
                   HCS_SYSTEM* computeSystem) {
                    ASSERT_STREQ(L"test_vm", id);

                    const auto config_no_whitespace = trim_whitespace(configuration);
                    const auto expected_no_whitespace = trim_whitespace(expected_vm_settings_json);

                    ASSERT_STREQ(expected_no_whitespace.c_str(), config_no_whitespace.c_str());
                    ASSERT_EQ(mock_operation_object, operation);
                    ASSERT_EQ(nullptr, securityDescriptor);
                    ASSERT_NE(nullptr, computeSystem);
                    ASSERT_EQ(nullptr, *computeSystem);
                    *computeSystem = mock_compute_system_object;
                },
                Return(NOERROR)));

        EXPECT_CALL(mock_close_compute_system, Call).WillOnce([](HCS_SYSTEM computeSystem) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
        });

        EXPECT_CALL(mock_local_free, Call)
            .WillOnce(DoAll([](HLOCAL ptr) { ASSERT_EQ(ptr, mock_success_msg); }, Return(nullptr)));

        logger_scope.mock_logger->expect_log(mpl::Level::debug, "HCSWrapper::HCSWrapper(...)");
        logger_scope.mock_logger->expect_log(
            mpl::Level::debug,
            "HCSWrapper::create_compute_system(...) > params: Compute System name: (test_vm) | vCPU count: (8) | "
            "Memory size: (16384 MiB) | cloud-init ISO path: () | VHDX path: ()");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "create_operation(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "wait_for_operation_result(...)");
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        uut_t uut{mock_api_table};
        hyperv::hcs::CreateComputeSystemParameters params{};
        params.name = "test_vm";
        params.cloudinit_iso_path = "";
        params.vhdx_path = "";
        params.memory_size_mb = 16384;
        params.processor_count = 8;

        const auto& [status, status_msg] = uut.create_compute_system(params);
        ASSERT_TRUE(status);
        ASSERT_FALSE(status_msg.empty());
        ASSERT_STREQ(status_msg.c_str(), mock_success_msg);
    }
}

// ---------------------------------------------------------

/**
 * Success scenario: Everything goes as expected.
 */
TEST_F(HyperVHCSAPI_UnitTests, create_compute_system_create_operation_fail)
{
    /******************************************************
     * Override the default mock functions.
     ******************************************************/
    ::testing::MockFunction<decltype(HcsCreateOperation)> mock_create_operation;

    mock_api_table.CreateOperation = mock_create_operation.AsStdFunction();

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_create_operation, Call)
            .WillOnce(DoAll(
                [](const void* context, HCS_OPERATION_COMPLETION callback) {
                    ASSERT_EQ(nullptr, context);
                    ASSERT_EQ(nullptr, callback);
                },
                Return(nullptr)));

        logger_scope.mock_logger->expect_log(mpl::Level::debug, "HCSWrapper::HCSWrapper(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "HCSWrapper::create_compute_system(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "create_operation(...)");
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        uut_t uut{mock_api_table};
        hyperv::hcs::CreateComputeSystemParameters params{};
        params.name = "test_vm";
        params.cloudinit_iso_path = "cloudinit iso path";
        params.vhdx_path = "virtual disk path";
        params.memory_size_mb = 16384;
        params.processor_count = 8;

        const auto& [status, status_msg] = uut.create_compute_system(params);
        ASSERT_FALSE(status);
        ASSERT_FALSE(status_msg.empty());
        ASSERT_STREQ(status_msg.c_str(), L"HcsCreateOperation failed.");
    }
}

// ---------------------------------------------------------

/**
 * Success scenario: Everything goes as expected.
 */
TEST_F(HyperVHCSAPI_UnitTests, create_compute_system_fail)
{
    /******************************************************
     * Override the default mock functions.
     ******************************************************/
    ::testing::MockFunction<decltype(HcsCreateOperation)> mock_create_operation;
    ::testing::MockFunction<decltype(HcsCloseOperation)> mock_close_operation;
    ::testing::MockFunction<decltype(HcsCreateComputeSystem)> mock_create_compute_system;

    mock_api_table.CreateOperation = mock_create_operation.AsStdFunction();
    mock_api_table.CloseOperation = mock_close_operation.AsStdFunction();
    mock_api_table.CreateComputeSystem = mock_create_compute_system.AsStdFunction();

    constexpr auto expected_vm_settings_json = LR"(
     {
         "SchemaVersion": {
             "Major": 2,
             "Minor": 1
         },
         "Owner": "Multipass",
         "ShouldTerminateOnLastHandleClosed": false,
         "VirtualMachine": {
             "Chipset": {
                 "Uefi": {
                     "BootThis": {
                         "DevicePath": "Primary disk",
                         "DiskNumber": 0,
                         "DeviceType": "ScsiDrive"
                     },
                     "Console": "ComPort1"
                 }
             },
             "ComputeTopology": {
                 "Memory": {
                     "Backing": "Virtual",
                     "SizeInMB": 16384
                 },
                 "Processor": {
                     "Count": 8
                 }
             },
             "Devices": {
                 "ComPorts": {
                     "0": {
                         "NamedPipe": "\\\\.\\pipe\\test_vm"
                     }
                 },
                 "Scsi": {
                     "cloud-init iso file": {
                         "Attachments": {
                             "0": {
                                 "Type": "Iso",
                                 "Path": "cloudinit iso path",
                                 "ReadOnly": true
                             }
                         }
                     },
                     "Primary disk": {
                         "Attachments": {
                             "0": {
                                 "Type": "VirtualDisk",
                                 "Path": "virtual disk path",
                                 "ReadOnly": false
                             }
                         }
                     },
                 }
             }
         }
     })";

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_create_operation, Call)
            .WillOnce(DoAll(
                [](const void* context, HCS_OPERATION_COMPLETION callback) {
                    ASSERT_EQ(nullptr, context);
                    ASSERT_EQ(nullptr, callback);
                },
                Return(mock_operation_object)));

        EXPECT_CALL(mock_close_operation, Call).WillOnce([](HCS_OPERATION op) {
            ASSERT_EQ(op, mock_operation_object);
        });

        EXPECT_CALL(mock_create_compute_system, Call)
            .WillOnce(DoAll(
                [](PCWSTR id,
                   PCWSTR configuration,
                   HCS_OPERATION operation,
                   const SECURITY_DESCRIPTOR* securityDescriptor,
                   HCS_SYSTEM* computeSystem) {
                    ASSERT_STREQ(L"test_vm", id);

                    const auto config_no_whitespace = trim_whitespace(configuration);
                    const auto expected_no_whitespace = trim_whitespace(expected_vm_settings_json);

                    ASSERT_STREQ(expected_no_whitespace.c_str(), config_no_whitespace.c_str());
                    ASSERT_EQ(mock_operation_object, operation);
                    ASSERT_EQ(nullptr, securityDescriptor);
                    ASSERT_NE(nullptr, computeSystem);
                    ASSERT_EQ(nullptr, *computeSystem);
                },
                Return(E_POINTER)));

        logger_scope.mock_logger->expect_log(mpl::Level::debug, "HCSWrapper::HCSWrapper(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "HCSWrapper::create_compute_system(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "create_operation(...)");
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        uut_t uut{mock_api_table};
        hyperv::hcs::CreateComputeSystemParameters params{};
        params.name = "test_vm";
        params.cloudinit_iso_path = "cloudinit iso path";
        params.vhdx_path = "virtual disk path";
        params.memory_size_mb = 16384;
        params.processor_count = 8;

        const auto& [status, status_msg] = uut.create_compute_system(params);
        ASSERT_FALSE(status);
        ASSERT_FALSE(status_msg.empty());
        ASSERT_STREQ(status_msg.c_str(), L"HcsCreateComputeSystem failed.");
    }
}

// ---------------------------------------------------------

/**
 * Success scenario: Everything goes as expected.
 */
TEST_F(HyperVHCSAPI_UnitTests, create_compute_system_wait_for_operation_fail)
{
    /******************************************************
     * Override the default mock functions.
     ******************************************************/
    ::testing::MockFunction<decltype(HcsCreateOperation)> mock_create_operation;
    ::testing::MockFunction<decltype(HcsCloseOperation)> mock_close_operation;
    ::testing::MockFunction<decltype(HcsWaitForOperationResult)> mock_wait_for_operation_result;
    ::testing::MockFunction<decltype(HcsCreateComputeSystem)> mock_create_compute_system;
    ::testing::MockFunction<decltype(HcsCloseComputeSystem)> mock_close_compute_system;
    ::testing::MockFunction<decltype(::LocalFree)> mock_local_free;

    mock_api_table.CreateOperation = mock_create_operation.AsStdFunction();
    mock_api_table.CloseOperation = mock_close_operation.AsStdFunction();
    mock_api_table.WaitForOperationResult = mock_wait_for_operation_result.AsStdFunction();
    mock_api_table.CreateComputeSystem = mock_create_compute_system.AsStdFunction();
    mock_api_table.CloseComputeSystem = mock_close_compute_system.AsStdFunction();
    mock_api_table.LocalFree = mock_local_free.AsStdFunction();

    constexpr auto expected_vm_settings_json = LR"(
     {
         "SchemaVersion": {
             "Major": 2,
             "Minor": 1
         },
         "Owner": "Multipass",
         "ShouldTerminateOnLastHandleClosed": false,
         "VirtualMachine": {
             "Chipset": {
                 "Uefi": {
                     "BootThis": {
                         "DevicePath": "Primary disk",
                         "DiskNumber": 0,
                         "DeviceType": "ScsiDrive"
                     },
                     "Console": "ComPort1"
                 }
             },
             "ComputeTopology": {
                 "Memory": {
                     "Backing": "Virtual",
                     "SizeInMB": 16384
                 },
                 "Processor": {
                     "Count": 8
                 }
             },
             "Devices": {
                 "ComPorts": {
                     "0": {
                         "NamedPipe": "\\\\.\\pipe\\test_vm"
                     }
                 },
                 "Scsi": {
                     "cloud-init iso file": {
                         "Attachments": {
                             "0": {
                                 "Type": "Iso",
                                 "Path": "cloudinit iso path",
                                 "ReadOnly": true
                             }
                         }
                     },
                     "Primary disk": {
                         "Attachments": {
                             "0": {
                                 "Type": "VirtualDisk",
                                 "Path": "virtual disk path",
                                 "ReadOnly": false
                             }
                         }
                     },
                 }
             }
         }
     })";

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_create_operation, Call)
            .WillOnce(DoAll(
                [](const void* context, HCS_OPERATION_COMPLETION callback) {
                    ASSERT_EQ(nullptr, context);
                    ASSERT_EQ(nullptr, callback);
                },
                Return(mock_operation_object)));

        EXPECT_CALL(mock_close_operation, Call).WillOnce([](HCS_OPERATION op) {
            ASSERT_EQ(op, mock_operation_object);
        });

        EXPECT_CALL(mock_wait_for_operation_result, Call)
            .WillOnce(DoAll(
                [](HCS_OPERATION operation, DWORD timeoutMs, PWSTR* resultDocument) {
                    ASSERT_EQ(operation, mock_operation_object);
                    ASSERT_EQ(timeoutMs, 240000);
                    ASSERT_NE(nullptr, resultDocument);
                    ASSERT_EQ(nullptr, *resultDocument);
                    *resultDocument = mock_error_msg;
                },
                Return(E_POINTER)));

        EXPECT_CALL(mock_create_compute_system, Call)
            .WillOnce(DoAll(
                [](PCWSTR id,
                   PCWSTR configuration,
                   HCS_OPERATION operation,
                   const SECURITY_DESCRIPTOR* securityDescriptor,
                   HCS_SYSTEM* computeSystem) {
                    ASSERT_STREQ(L"test_vm", id);

                    const auto config_no_whitespace = trim_whitespace(configuration);
                    const auto expected_no_whitespace = trim_whitespace(expected_vm_settings_json);

                    ASSERT_STREQ(expected_no_whitespace.c_str(), config_no_whitespace.c_str());
                    ASSERT_EQ(mock_operation_object, operation);
                    ASSERT_EQ(nullptr, securityDescriptor);
                    ASSERT_NE(nullptr, computeSystem);
                    ASSERT_EQ(nullptr, *computeSystem);
                    *computeSystem = mock_compute_system_object;
                },
                Return(NOERROR)));

        EXPECT_CALL(mock_close_compute_system, Call).WillOnce([](HCS_SYSTEM computeSystem) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
        });

        EXPECT_CALL(mock_local_free, Call)
            .WillOnce(DoAll([](HLOCAL ptr) { ASSERT_EQ(ptr, mock_error_msg); }, Return(nullptr)));

        logger_scope.mock_logger->expect_log(mpl::Level::debug, "HCSWrapper::HCSWrapper(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "HCSWrapper::create_compute_system(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "create_operation(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "wait_for_operation_result(...)");
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        uut_t uut{mock_api_table};
        hyperv::hcs::CreateComputeSystemParameters params{};
        params.name = "test_vm";
        params.cloudinit_iso_path = "cloudinit iso path";
        params.vhdx_path = "virtual disk path";
        params.memory_size_mb = 16384;
        params.processor_count = 8;

        const auto& [status, status_msg] = uut.create_compute_system(params);
        ASSERT_FALSE(status);
        ASSERT_FALSE(status_msg.empty());
        ASSERT_STREQ(status_msg.c_str(), mock_error_msg);
    }
}

// ---------------------------------------------------------

/**
 * Success scenario: Everything goes as expected.
 */
TEST_F(HyperVHCSAPI_UnitTests, grant_vm_access_success)
{
    /******************************************************
     * Override the default mock functions.
     ******************************************************/
    ::testing::MockFunction<decltype(HcsGrantVmAccess)> mock_grant_vm_access;

    mock_api_table.GrantVmAccess = mock_grant_vm_access.AsStdFunction();

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_grant_vm_access, Call)
            .WillOnce(DoAll(
                [](PCWSTR vmId, PCWSTR filePath) {
                    ASSERT_NE(nullptr, vmId);
                    ASSERT_NE(nullptr, filePath);
                    ASSERT_STREQ(vmId, L"test_vm");
                    ASSERT_STREQ(filePath, L"this is a path");
                },
                Return(NOERROR)));

        logger_scope.mock_logger->expect_log(mpl::Level::debug, "HCSWrapper::HCSWrapper(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::debug,
                                             "grant_vm_access(...) > name: (test_vm), file_path: (this is a path)");
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        uut_t uut{mock_api_table};

        const auto& [status, status_msg] = uut.grant_vm_access("test_vm", "this is a path");
        ASSERT_TRUE(status);
        ASSERT_TRUE(status_msg.empty());
    }
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, grant_vm_access_fail)
{
    /******************************************************
     * Override the default mock functions.
     ******************************************************/
    ::testing::MockFunction<decltype(HcsGrantVmAccess)> mock_grant_vm_access;

    mock_api_table.GrantVmAccess = mock_grant_vm_access.AsStdFunction();

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_grant_vm_access, Call)
            .WillOnce(DoAll(
                [](PCWSTR vmId, PCWSTR filePath) {
                    ASSERT_NE(nullptr, vmId);
                    ASSERT_NE(nullptr, filePath);
                    ASSERT_STREQ(vmId, L"test_vm");
                    ASSERT_STREQ(filePath, L"this is a path");
                },
                Return(E_POINTER)));

        logger_scope.mock_logger->expect_log(mpl::Level::debug, "HCSWrapper::HCSWrapper(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::debug,
                                             "grant_vm_access(...) > name: (test_vm), file_path: (this is a path)");
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        uut_t uut{mock_api_table};

        const auto& [status, status_msg] = uut.grant_vm_access("test_vm", "this is a path");
        ASSERT_FALSE(status);
        ASSERT_FALSE(status_msg.empty());
        ASSERT_STREQ(status_msg.c_str(), L"GrantVmAccess failed!");
    }
}

// ---------------------------------------------------------

/**
 * Success scenario: Everything goes as expected.
 */
TEST_F(HyperVHCSAPI_UnitTests, revoke_vm_access_success)
{
    /******************************************************
     * Override the default mock functions.
     ******************************************************/
    ::testing::MockFunction<decltype(HcsGrantVmAccess)> mock_revoke_vm_access;

    mock_api_table.RevokeVmAccess = mock_revoke_vm_access.AsStdFunction();

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_revoke_vm_access, Call)
            .WillOnce(DoAll(
                [](PCWSTR vmId, PCWSTR filePath) {
                    ASSERT_NE(nullptr, vmId);
                    ASSERT_NE(nullptr, filePath);
                    ASSERT_STREQ(vmId, L"test_vm");
                    ASSERT_STREQ(filePath, L"this is a path");
                },
                Return(NOERROR)));

        logger_scope.mock_logger->expect_log(mpl::Level::debug, "HCSWrapper::HCSWrapper(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::debug,
                                             "revoke_vm_access(...) > name: (test_vm), file_path: (this is a path)");
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        uut_t uut{mock_api_table};

        const auto& [status, status_msg] = uut.revoke_vm_access("test_vm", "this is a path");
        ASSERT_TRUE(status);
        ASSERT_TRUE(status_msg.empty());
    }
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, revoke_vm_access_fail)
{
    /******************************************************
     * Override the default mock functions.
     ******************************************************/
    ::testing::MockFunction<decltype(HcsGrantVmAccess)> mock_revoke_vm_access;

    mock_api_table.RevokeVmAccess = mock_revoke_vm_access.AsStdFunction();

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_revoke_vm_access, Call)
            .WillOnce(DoAll(
                [](PCWSTR vmId, PCWSTR filePath) {
                    ASSERT_NE(nullptr, vmId);
                    ASSERT_NE(nullptr, filePath);
                    ASSERT_STREQ(vmId, L"test_vm");
                    ASSERT_STREQ(filePath, L"this is a path");
                },
                Return(E_POINTER)));
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "HCSWrapper::HCSWrapper(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::debug,
                                             "revoke_vm_access(...) > name: (test_vm), file_path: (this is a path)");
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        uut_t uut{mock_api_table};

        const auto& [status, status_msg] = uut.revoke_vm_access("test_vm", "this is a path");
        ASSERT_FALSE(status);
        ASSERT_FALSE(status_msg.empty());
        ASSERT_STREQ(status_msg.c_str(), L"RevokeVmAccess failed!");
    }
}

// ---------------------------------------------------------

//
// Below are the skeleton test cases for the functions that are following
// the same pattern.
//

template <typename TargetFnSig, typename UutCallableT, typename MockCallableT, typename ApiFnT>
void HyperVHCSAPI_UnitTests::generic_operation_happy_path(ApiFnT& target_api_function,
                                                          UutCallableT uut_callback,
                                                          MockCallableT mock_callback,
                                                          PWSTR operation_result_document,
                                                          PWSTR expected_status_msg)
{
    /******************************************************
     * Override the default mock functions.
     ******************************************************/
    ::testing::MockFunction<decltype(HcsCreateOperation)> mock_create_operation;
    ::testing::MockFunction<decltype(HcsCloseOperation)> mock_close_operation;
    ::testing::MockFunction<decltype(HcsWaitForOperationResult)> mock_wait_for_operation_result;
    ::testing::MockFunction<decltype(HcsOpenComputeSystem)> mock_open_compute_system;
    ::testing::MockFunction<decltype(HcsCloseComputeSystem)> mock_close_compute_system;
    ::testing::MockFunction<TargetFnSig> mock_target_function;
    ::testing::MockFunction<decltype(::LocalFree)> mock_local_free;

    mock_api_table.CreateOperation = mock_create_operation.AsStdFunction();
    mock_api_table.CloseOperation = mock_close_operation.AsStdFunction();
    mock_api_table.WaitForOperationResult = mock_wait_for_operation_result.AsStdFunction();
    mock_api_table.OpenComputeSystem = mock_open_compute_system.AsStdFunction();
    mock_api_table.CloseComputeSystem = mock_close_compute_system.AsStdFunction();
    target_api_function = mock_target_function.AsStdFunction();

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_create_operation, Call)
            .WillOnce(DoAll(
                [](const void* context, HCS_OPERATION_COMPLETION callback) {
                    ASSERT_EQ(nullptr, context);
                    ASSERT_EQ(nullptr, callback);
                },
                Return(mock_operation_object)));

        EXPECT_CALL(mock_close_operation, Call).WillOnce([](HCS_OPERATION op) {
            ASSERT_EQ(op, mock_operation_object);
        });

        EXPECT_CALL(mock_wait_for_operation_result, Call)
            .WillOnce(DoAll(
                [operation_result_document](HCS_OPERATION operation, DWORD timeoutMs, PWSTR* resultDocument) {
                    ASSERT_EQ(operation, mock_operation_object);
                    ASSERT_EQ(timeoutMs, 240000);
                    ASSERT_NE(nullptr, resultDocument);
                    ASSERT_EQ(nullptr, *resultDocument);
                    *resultDocument = operation_result_document;
                },
                Return(NOERROR)));

        EXPECT_CALL(mock_open_compute_system, Call)
            .WillOnce(DoAll(
                [&](PCWSTR id, DWORD requestedAccess, HCS_SYSTEM* computeSystem) {
                    ASSERT_STREQ(id, L"test_vm");
                    ASSERT_EQ(requestedAccess, GENERIC_ALL);
                    ASSERT_NE(nullptr, computeSystem);
                    ASSERT_EQ(nullptr, *computeSystem);
                    *computeSystem = mock_compute_system_object;
                },
                Return(NOERROR)));

        EXPECT_CALL(mock_target_function, Call).WillOnce(DoAll(mock_callback, Return(NOERROR)));

        EXPECT_CALL(mock_close_compute_system, Call).WillOnce([](HCS_SYSTEM computeSystem) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
        });

        if (operation_result_document)
        {
            mock_api_table.LocalFree = mock_local_free.AsStdFunction();
            EXPECT_CALL(mock_local_free, Call)
                .WillOnce(DoAll([operation_result_document](HLOCAL ptr) { ASSERT_EQ(operation_result_document, ptr); },
                                Return(nullptr)));
        }

        logger_scope.mock_logger->expect_log(mpl::Level::debug, "HCSWrapper::HCSWrapper(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "open_host_compute_system(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "create_operation(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "perform_hcs_operation(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "wait_for_operation_result(...)");
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        uut_t uut{mock_api_table};

        const auto& [status, status_msg] = uut_callback(uut);
        ASSERT_TRUE(status);

        if (nullptr == expected_status_msg)
        {
            ASSERT_TRUE(status_msg.empty());
        }
        else
        {
            ASSERT_STREQ(status_msg.c_str(), expected_status_msg);
        }
    }
}

template <typename TargetFnSig, typename UutCallableT, typename ApiFnT>
void HyperVHCSAPI_UnitTests::generic_operation_hcs_open_fail(ApiFnT& target_api_function,
                                                             UutCallableT uut_callback,
                                                             PWSTR expected_status_msg)
{
    /******************************************************
     * Override the default mock functions.
     ******************************************************/

    ::testing::MockFunction<decltype(HcsOpenComputeSystem)> mock_open_compute_system;

    mock_api_table.OpenComputeSystem = mock_open_compute_system.AsStdFunction();

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {

        EXPECT_CALL(mock_open_compute_system, Call)
            .WillOnce(DoAll(
                [&](PCWSTR id, DWORD requestedAccess, HCS_SYSTEM* computeSystem) {
                    ASSERT_STREQ(id, L"test_vm");
                    ASSERT_EQ(requestedAccess, GENERIC_ALL);
                    ASSERT_NE(nullptr, computeSystem);
                    ASSERT_EQ(nullptr, *computeSystem);
                },
                Return(E_POINTER)));

        logger_scope.mock_logger->expect_log(mpl::Level::debug, "HCSWrapper::HCSWrapper(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "open_host_compute_system(...) > name: (test_vm)");
        logger_scope.mock_logger->expect_log(
            mpl::Level::error,
            "open_host_compute_system(...) > failed to open (test_vm), result code: (0x80004003)");
        logger_scope.mock_logger->expect_log(mpl::Level::error,
                                             "perform_hcs_operation(...) > HcsOpenComputeSystem failed!");
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        uut_t uut{mock_api_table};

        const auto& [status, status_msg] = uut_callback(uut);
        ASSERT_FALSE(status);

        if (nullptr == expected_status_msg)
        {
            ASSERT_TRUE(status_msg.empty());
        }
        else
        {
            ASSERT_FALSE(status_msg.empty());
            ASSERT_STREQ(status_msg.c_str(), expected_status_msg);
        }
    }
}

template <typename TargetFnSig, typename UutCallableT, typename ApiFnT>
void HyperVHCSAPI_UnitTests::generic_operation_create_operation_fail(ApiFnT& target_api_function,
                                                                     UutCallableT uut_callback,
                                                                     PWSTR expected_status_msg)
{

    /******************************************************
     * Override the default mock functions.
     ******************************************************/
    ::testing::MockFunction<decltype(HcsCreateOperation)> mock_create_operation;
    ::testing::MockFunction<decltype(HcsOpenComputeSystem)> mock_open_compute_system;
    ::testing::MockFunction<decltype(HcsCloseComputeSystem)> mock_close_compute_system;

    mock_api_table.OpenComputeSystem = mock_open_compute_system.AsStdFunction();
    mock_api_table.CreateOperation = mock_create_operation.AsStdFunction();
    mock_api_table.CloseComputeSystem = mock_close_compute_system.AsStdFunction();

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {

        EXPECT_CALL(mock_open_compute_system, Call)
            .WillOnce(DoAll(
                [&](PCWSTR id, DWORD requestedAccess, HCS_SYSTEM* computeSystem) {
                    ASSERT_STREQ(id, L"test_vm");
                    ASSERT_EQ(requestedAccess, GENERIC_ALL);
                    ASSERT_NE(nullptr, computeSystem);
                    ASSERT_EQ(nullptr, *computeSystem);
                    *computeSystem = mock_compute_system_object;
                },
                Return(NOERROR)));

        EXPECT_CALL(mock_create_operation, Call)
            .WillOnce(DoAll(
                [](const void* context, HCS_OPERATION_COMPLETION callback) {
                    ASSERT_EQ(nullptr, context);
                    ASSERT_EQ(nullptr, callback);
                },
                Return(nullptr)));

        EXPECT_CALL(mock_close_compute_system, Call).WillOnce([](HCS_SYSTEM computeSystem) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
        });

        logger_scope.mock_logger->expect_log(mpl::Level::debug, "HCSWrapper::HCSWrapper(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "open_host_compute_system(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "create_operation(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::error,
                                             "perform_hcs_operation(...) > HcsCreateOperation failed!");
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        uut_t uut{mock_api_table};

        const auto& [status, status_msg] = uut_callback(uut);
        ASSERT_FALSE(status);

        if (nullptr == expected_status_msg)
        {
            ASSERT_TRUE(status_msg.empty());
        }
        else
        {
            ASSERT_FALSE(status_msg.empty());
            ASSERT_STREQ(status_msg.c_str(), expected_status_msg);
        }
    }
}

template <typename TargetFnSig, typename UutCallableT, typename MockCallableT, typename ApiFnT>
void HyperVHCSAPI_UnitTests::generic_operation_fail(ApiFnT& target_api_function,
                                                    UutCallableT uut_callback,
                                                    MockCallableT mock_callback,
                                                    PWSTR expected_status_msg)
{
    /******************************************************
     * Override the default mock functions.
     ******************************************************/
    ::testing::MockFunction<decltype(HcsCreateOperation)> mock_create_operation;
    ::testing::MockFunction<decltype(HcsCloseOperation)> mock_close_operation;
    ::testing::MockFunction<decltype(HcsOpenComputeSystem)> mock_open_compute_system;
    ::testing::MockFunction<decltype(HcsCloseComputeSystem)> mock_close_compute_system;
    ::testing::MockFunction<TargetFnSig> mock_target_function;

    mock_api_table.CreateOperation = mock_create_operation.AsStdFunction();
    mock_api_table.CloseOperation = mock_close_operation.AsStdFunction();
    mock_api_table.OpenComputeSystem = mock_open_compute_system.AsStdFunction();
    mock_api_table.CloseComputeSystem = mock_close_compute_system.AsStdFunction();
    target_api_function = mock_target_function.AsStdFunction();

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_create_operation, Call)
            .WillOnce(DoAll(
                [](const void* context, HCS_OPERATION_COMPLETION callback) {
                    ASSERT_EQ(nullptr, context);
                    ASSERT_EQ(nullptr, callback);
                },
                Return(mock_operation_object)));

        EXPECT_CALL(mock_close_operation, Call).WillOnce([](HCS_OPERATION op) {
            ASSERT_EQ(op, mock_operation_object);
        });

        EXPECT_CALL(mock_open_compute_system, Call)
            .WillOnce(DoAll(
                [&](PCWSTR id, DWORD requestedAccess, HCS_SYSTEM* computeSystem) {
                    ASSERT_STREQ(id, L"test_vm");
                    ASSERT_EQ(requestedAccess, GENERIC_ALL);
                    ASSERT_NE(nullptr, computeSystem);
                    ASSERT_EQ(nullptr, *computeSystem);
                    *computeSystem = mock_compute_system_object;
                },
                Return(NOERROR)));

        EXPECT_CALL(mock_target_function, Call).WillOnce(DoAll(mock_callback, Return(E_POINTER)));

        EXPECT_CALL(mock_close_compute_system, Call).WillOnce([](HCS_SYSTEM computeSystem) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
        });

        logger_scope.mock_logger->expect_log(mpl::Level::debug, "HCSWrapper::HCSWrapper(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "open_host_compute_system(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "create_operation(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::error, "perform_hcs_operation(...) > Operation failed!");
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        uut_t uut{mock_api_table};

        const auto& [status, status_msg] = uut_callback(uut);
        ASSERT_FALSE(status);
        if (nullptr == expected_status_msg)
        {
            ASSERT_TRUE(status_msg.empty());
        }
        else
        {
            ASSERT_FALSE(status_msg.empty());
            ASSERT_STREQ(status_msg.c_str(), expected_status_msg);
        }
    }
}

template <typename TargetFnSig, typename UutCallableT, typename MockCallableT, typename ApiFnT>
void HyperVHCSAPI_UnitTests::generic_operation_wait_for_operation_fail(ApiFnT& target_api_function,
                                                                       UutCallableT uut_callback,
                                                                       MockCallableT mock_callback,
                                                                       PWSTR operation_result_document,
                                                                       PWSTR expected_status_msg)
{
    /******************************************************
     * Override the default mock functions.
     ******************************************************/
    ::testing::MockFunction<decltype(HcsCreateOperation)> mock_create_operation;
    ::testing::MockFunction<decltype(HcsCloseOperation)> mock_close_operation;
    ::testing::MockFunction<decltype(HcsWaitForOperationResult)> mock_wait_for_operation_result;
    ::testing::MockFunction<decltype(HcsOpenComputeSystem)> mock_open_compute_system;
    ::testing::MockFunction<decltype(HcsCloseComputeSystem)> mock_close_compute_system;
    ::testing::MockFunction<TargetFnSig> mock_target_function;
    ::testing::MockFunction<decltype(::LocalFree)> mock_local_free;

    mock_api_table.CreateOperation = mock_create_operation.AsStdFunction();
    mock_api_table.CloseOperation = mock_close_operation.AsStdFunction();
    mock_api_table.WaitForOperationResult = mock_wait_for_operation_result.AsStdFunction();
    mock_api_table.OpenComputeSystem = mock_open_compute_system.AsStdFunction();
    mock_api_table.CloseComputeSystem = mock_close_compute_system.AsStdFunction();
    target_api_function = mock_target_function.AsStdFunction();

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_create_operation, Call)
            .WillOnce(DoAll(
                [](const void* context, HCS_OPERATION_COMPLETION callback) {
                    ASSERT_EQ(nullptr, context);
                    ASSERT_EQ(nullptr, callback);
                },
                Return(mock_operation_object)));

        EXPECT_CALL(mock_close_operation, Call).WillOnce([](HCS_OPERATION op) {
            ASSERT_EQ(op, mock_operation_object);
        });

        EXPECT_CALL(mock_wait_for_operation_result, Call)
            .WillOnce(DoAll(
                [operation_result_document](HCS_OPERATION operation, DWORD timeoutMs, PWSTR* resultDocument) {
                    ASSERT_EQ(operation, mock_operation_object);
                    ASSERT_EQ(timeoutMs, 240000);
                    ASSERT_NE(nullptr, resultDocument);
                    ASSERT_EQ(nullptr, *resultDocument);
                    *resultDocument = operation_result_document;
                },
                Return(E_POINTER)));

        EXPECT_CALL(mock_open_compute_system, Call)
            .WillOnce(DoAll(
                [&](PCWSTR id, DWORD requestedAccess, HCS_SYSTEM* computeSystem) {
                    ASSERT_STREQ(id, L"test_vm");
                    ASSERT_EQ(requestedAccess, GENERIC_ALL);
                    ASSERT_NE(nullptr, computeSystem);
                    ASSERT_EQ(nullptr, *computeSystem);
                    *computeSystem = mock_compute_system_object;
                },
                Return(NOERROR)));

        EXPECT_CALL(mock_target_function, Call).WillOnce(DoAll(mock_callback, Return(NOERROR)));

        EXPECT_CALL(mock_close_compute_system, Call).WillOnce([](HCS_SYSTEM computeSystem) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
        });

        if (operation_result_document)
        {
            mock_api_table.LocalFree = mock_local_free.AsStdFunction();
            EXPECT_CALL(mock_local_free, Call)
                .WillOnce(DoAll([operation_result_document](HLOCAL ptr) { ASSERT_EQ(operation_result_document, ptr); },
                                Return(nullptr)));
        }

        logger_scope.mock_logger->expect_log(mpl::Level::debug, "HCSWrapper::HCSWrapper(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "open_host_compute_system(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "create_operation(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "perform_hcs_operation(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "wait_for_operation_result(...)");
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        uut_t uut{mock_api_table};

        const auto& [status, status_msg] = uut_callback(uut);
        ASSERT_FALSE(status);

        if (nullptr == expected_status_msg)
        {
            ASSERT_TRUE(status_msg.empty());
        }
        else
        {
            ASSERT_FALSE(status_msg.empty());
            ASSERT_STREQ(status_msg.c_str(), expected_status_msg);
        }
    }
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, start_compute_system_happy_path)
{
    generic_operation_happy_path<decltype(HcsStartComputeSystem)>(
        mock_api_table.StartComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "start_compute_system(...) > name: (test_vm)");
            return wrapper.start_compute_system("test_vm");
        },
        [](HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR options) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
            ASSERT_EQ(mock_operation_object, operation);
            ASSERT_EQ(options, nullptr);
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, start_compute_system_hcs_open_fail)
{
    generic_operation_hcs_open_fail<decltype(HcsStartComputeSystem)>(
        mock_api_table.StartComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "start_compute_system(...)");
            return wrapper.start_compute_system("test_vm");
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, start_compute_system_create_operation_fail)
{

    generic_operation_create_operation_fail<decltype(HcsStartComputeSystem)>(
        mock_api_table.StartComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "start_compute_system(...)");
            return wrapper.start_compute_system("test_vm");
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, start_compute_system_fail)
{
    generic_operation_fail<decltype(HcsStartComputeSystem)>(
        mock_api_table.StartComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "start_compute_system(...)");
            return wrapper.start_compute_system("test_vm");
        },
        [](HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR options) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
            ASSERT_EQ(mock_operation_object, operation);
            ASSERT_EQ(options, nullptr);
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, start_compute_system_wait_for_operation_result_fail)
{
    generic_operation_wait_for_operation_fail<decltype(HcsStartComputeSystem)>(
        mock_api_table.StartComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "start_compute_system(...)");
            return wrapper.start_compute_system("test_vm");
        },
        [](HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR options) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
            ASSERT_EQ(mock_operation_object, operation);
            ASSERT_EQ(options, nullptr);
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, shutdown_compute_system_happy_path)
{
    generic_operation_happy_path<decltype(HcsShutDownComputeSystem)>(
        mock_api_table.ShutDownComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "shutdown_compute_system(...) > name: (test_vm)");
            return wrapper.shutdown_compute_system("test_vm");
        },
        [](HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR options) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
            ASSERT_EQ(mock_operation_object, operation);
            ASSERT_EQ(options, nullptr);
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, shutdown_compute_system_hcs_open_fail)
{
    generic_operation_hcs_open_fail<decltype(HcsShutDownComputeSystem)>(
        mock_api_table.ShutDownComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "shutdown_compute_system(...)");
            return wrapper.shutdown_compute_system("test_vm");
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, shutdown_compute_system_create_operation_fail)
{

    generic_operation_create_operation_fail<decltype(HcsShutDownComputeSystem)>(
        mock_api_table.ShutDownComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "shutdown_compute_system(...)");
            return wrapper.shutdown_compute_system("test_vm");
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, shutdown_compute_system_fail)
{
    generic_operation_fail<decltype(HcsShutDownComputeSystem)>(
        mock_api_table.ShutDownComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "shutdown_compute_system(...)");
            return wrapper.shutdown_compute_system("test_vm");
        },
        [](HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR options) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
            ASSERT_EQ(mock_operation_object, operation);
            ASSERT_EQ(options, nullptr);
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, shutdown_compute_system_wait_for_operation_result_fail)
{
    generic_operation_wait_for_operation_fail<decltype(HcsShutDownComputeSystem)>(
        mock_api_table.ShutDownComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "shutdown_compute_system(...)");
            return wrapper.shutdown_compute_system("test_vm");
        },
        [](HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR options) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
            ASSERT_EQ(mock_operation_object, operation);
            ASSERT_EQ(options, nullptr);
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, terminate_compute_system_happy_path)
{
    generic_operation_happy_path<decltype(HcsTerminateComputeSystem)>(
        mock_api_table.TerminateComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "terminate_compute_system(...) > name: (test_vm)");
            return wrapper.terminate_compute_system("test_vm");
        },
        [](HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR options) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
            ASSERT_EQ(mock_operation_object, operation);
            ASSERT_EQ(options, nullptr);
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, terminate_compute_system_hcs_open_fail)
{
    generic_operation_hcs_open_fail<decltype(HcsTerminateComputeSystem)>(
        mock_api_table.TerminateComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "terminate_compute_system(...)");
            return wrapper.terminate_compute_system("test_vm");
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, terminate_compute_system_create_operation_fail)
{

    generic_operation_create_operation_fail<decltype(HcsTerminateComputeSystem)>(
        mock_api_table.TerminateComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "terminate_compute_system(...)");
            return wrapper.terminate_compute_system("test_vm");
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, terminate_compute_system_fail)
{
    generic_operation_fail<decltype(HcsTerminateComputeSystem)>(
        mock_api_table.TerminateComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "terminate_compute_system(...)");
            return wrapper.terminate_compute_system("test_vm");
        },
        [](HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR options) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
            ASSERT_EQ(mock_operation_object, operation);
            ASSERT_EQ(options, nullptr);
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, terminate_compute_system_wait_for_operation_result_fail)
{
    generic_operation_wait_for_operation_fail<decltype(HcsTerminateComputeSystem)>(
        mock_api_table.TerminateComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "terminate_compute_system(...)");
            return wrapper.terminate_compute_system("test_vm");
        },
        [](HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR options) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
            ASSERT_EQ(mock_operation_object, operation);
            ASSERT_EQ(options, nullptr);
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, pause_compute_system_happy_path)
{
    static constexpr wchar_t expected_pause_option[] = LR"(
        {
            "SuspensionLevel": "Suspend",
            "HostedNotification": {
                "Reason": "Save"
            }
        })";

    generic_operation_happy_path<decltype(HcsPauseComputeSystem)>(
        mock_api_table.PauseComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "pause_compute_system(...) > name: (test_vm)");
            return wrapper.pause_compute_system("test_vm");
        },
        [](HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR options) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
            ASSERT_EQ(mock_operation_object, operation);
            const auto options_no_whitespace = trim_whitespace(options);
            const auto expected_options_no_whitespace = trim_whitespace(expected_pause_option);
            ASSERT_STREQ(options_no_whitespace.c_str(), expected_options_no_whitespace.c_str());
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, pause_compute_system_hcs_open_fail)
{
    generic_operation_hcs_open_fail<decltype(HcsPauseComputeSystem)>(
        mock_api_table.PauseComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "pause_compute_system(...)");
            return wrapper.pause_compute_system("test_vm");
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, pause_compute_system_create_operation_fail)
{

    generic_operation_create_operation_fail<decltype(HcsPauseComputeSystem)>(
        mock_api_table.PauseComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "pause_compute_system(...)");
            return wrapper.pause_compute_system("test_vm");
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, pause_compute_system_fail)
{
    static constexpr wchar_t expected_pause_option[] = LR"(
        {
            "SuspensionLevel": "Suspend",
            "HostedNotification": {
                "Reason": "Save"
            }
        })";
    generic_operation_fail<decltype(HcsPauseComputeSystem)>(
        mock_api_table.PauseComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "pause_compute_system(...)");
            return wrapper.pause_compute_system("test_vm");
        },
        [](HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR options) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
            ASSERT_EQ(mock_operation_object, operation);
            const auto options_no_whitespace = trim_whitespace(options);
            const auto expected_options_no_whitespace = trim_whitespace(expected_pause_option);
            ASSERT_STREQ(options_no_whitespace.c_str(), expected_options_no_whitespace.c_str());
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, pause_compute_system_wait_for_operation_result_fail)
{
    static constexpr wchar_t expected_pause_option[] = LR"(
        {
            "SuspensionLevel": "Suspend",
            "HostedNotification": {
                "Reason": "Save"
            }
        })";
    generic_operation_wait_for_operation_fail<decltype(HcsPauseComputeSystem)>(
        mock_api_table.PauseComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "pause_compute_system(...)");
            return wrapper.pause_compute_system("test_vm");
        },
        [](HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR options) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
            ASSERT_EQ(mock_operation_object, operation);
            const auto options_no_whitespace = trim_whitespace(options);
            const auto expected_options_no_whitespace = trim_whitespace(expected_pause_option);
            ASSERT_STREQ(options_no_whitespace.c_str(), expected_options_no_whitespace.c_str());
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, resume_compute_system_happy_path)
{
    generic_operation_happy_path<decltype(HcsResumeComputeSystem)>(
        mock_api_table.ResumeComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "resume_compute_system(...) > name: (test_vm)");
            return wrapper.resume_compute_system("test_vm");
        },
        [](HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR options) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
            ASSERT_EQ(mock_operation_object, operation);
            ASSERT_EQ(options, nullptr);
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, resume_compute_system_hcs_open_fail)
{
    generic_operation_hcs_open_fail<decltype(HcsResumeComputeSystem)>(
        mock_api_table.ResumeComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "resume_compute_system(...)");
            return wrapper.resume_compute_system("test_vm");
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, resume_compute_system_create_operation_fail)
{

    generic_operation_create_operation_fail<decltype(HcsResumeComputeSystem)>(
        mock_api_table.ResumeComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "resume_compute_system(...)");
            return wrapper.resume_compute_system("test_vm");
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, resume_compute_system_fail)
{
    generic_operation_fail<decltype(HcsResumeComputeSystem)>(
        mock_api_table.ResumeComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "resume_compute_system(...)");
            return wrapper.resume_compute_system("test_vm");
        },
        [](HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR options) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
            ASSERT_EQ(mock_operation_object, operation);
            ASSERT_EQ(options, nullptr);
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, resume_compute_system_wait_for_operation_result_fail)
{
    generic_operation_wait_for_operation_fail<decltype(HcsResumeComputeSystem)>(
        mock_api_table.ResumeComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "resume_compute_system(...)");
            return wrapper.resume_compute_system("test_vm");
        },
        [](HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR options) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
            ASSERT_EQ(mock_operation_object, operation);
            ASSERT_EQ(options, nullptr);
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, add_endpoint_to_compute_system_happy_path)
{
    constexpr auto expected_modify_compute_system_configuration = LR"(
        {
            "ResourcePath": "VirtualMachine/Devices/NetworkAdapters/{288cc1ac-8f31-4a09-9e90-30ad0bcfdbca}",
            "RequestType": "Add",
            "Settings": {
                "EndpointId": "288cc1ac-8f31-4a09-9e90-30ad0bcfdbca",
                "MacAddress": "00:00:00:00:00:00",
                "InstanceId": "288cc1ac-8f31-4a09-9e90-30ad0bcfdbca"
            }
        })";

    generic_operation_happy_path<decltype(HcsModifyComputeSystem)>(
        mock_api_table.ModifyComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(
                mpl::Level::debug,
                "add_endpoint(...) > params: Host Compute System Name: (test_vm) | Endpoint GUID: "
                "(288cc1ac-8f31-4a09-9e90-30ad0bcfdbca) | NIC MAC Address: (00:00:00:00:00:00)");
            hyperv::hcs::AddEndpointParameters params{};
            params.endpoint_guid = "288cc1ac-8f31-4a09-9e90-30ad0bcfdbca";
            params.nic_mac_address = "00:00:00:00:00:00";
            params.target_compute_system_name = "test_vm";
            return wrapper.add_endpoint(params);
        },
        [](HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR configuration, HANDLE identity) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
            ASSERT_EQ(mock_operation_object, operation);
            const auto options_no_whitespace = trim_whitespace(configuration);
            const auto expected_options_no_whitespace = trim_whitespace(expected_modify_compute_system_configuration);
            ASSERT_STREQ(options_no_whitespace.c_str(), expected_options_no_whitespace.c_str());
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, add_endpoint_to_compute_system_hcs_open_fail)
{
    generic_operation_hcs_open_fail<decltype(HcsModifyComputeSystem)>(
        mock_api_table.ModifyComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "add_endpoint(...)");
            hyperv::hcs::AddEndpointParameters params{};
            params.target_compute_system_name = "test_vm";
            return wrapper.add_endpoint(params);
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, add_endpoint_to_compute_system_create_operation_fail)
{
    generic_operation_create_operation_fail<decltype(HcsModifyComputeSystem)>(
        mock_api_table.ModifyComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "add_endpoint(...)");
            hyperv::hcs::AddEndpointParameters params{};
            params.target_compute_system_name = "test_vm";
            return wrapper.add_endpoint(params);
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, add_endpoint_to_compute_system_fail)
{
    constexpr auto expected_modify_compute_system_configuration = LR"(
        {
            "ResourcePath": "VirtualMachine/Devices/NetworkAdapters/{288cc1ac-8f31-4a09-9e90-30ad0bcfdbca}",
            "RequestType": "Add",
            "Settings": {
                "EndpointId": "288cc1ac-8f31-4a09-9e90-30ad0bcfdbca",
                "MacAddress": "00:00:00:00:00:00",
                "InstanceId": "288cc1ac-8f31-4a09-9e90-30ad0bcfdbca"
            }
        })";

    generic_operation_fail<decltype(HcsModifyComputeSystem)>(
        mock_api_table.ModifyComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "add_endpoint(...)");
            hyperv::hcs::AddEndpointParameters params{};
            params.endpoint_guid = "288cc1ac-8f31-4a09-9e90-30ad0bcfdbca";
            params.nic_mac_address = "00:00:00:00:00:00";
            params.target_compute_system_name = "test_vm";
            return wrapper.add_endpoint(params);
        },
        [](HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR configuration, HANDLE identity) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
            ASSERT_EQ(mock_operation_object, operation);
            const auto options_no_whitespace = trim_whitespace(configuration);
            const auto expected_options_no_whitespace = trim_whitespace(expected_modify_compute_system_configuration);
            ASSERT_STREQ(options_no_whitespace.c_str(), expected_options_no_whitespace.c_str());
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, add_endpoint_to_compute_system_wait_for_operation_result_fail)
{
    constexpr auto expected_modify_compute_system_configuration = LR"(
        {
            "ResourcePath": "VirtualMachine/Devices/NetworkAdapters/{288cc1ac-8f31-4a09-9e90-30ad0bcfdbca}",
            "RequestType": "Add",
            "Settings": {
                "EndpointId": "288cc1ac-8f31-4a09-9e90-30ad0bcfdbca",
                "MacAddress": "00:00:00:00:00:00",
                "InstanceId": "288cc1ac-8f31-4a09-9e90-30ad0bcfdbca"
            }
        })";

    generic_operation_wait_for_operation_fail<decltype(HcsModifyComputeSystem)>(
        mock_api_table.ModifyComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "add_endpoint(...)");
            hyperv::hcs::AddEndpointParameters params{};
            params.endpoint_guid = "288cc1ac-8f31-4a09-9e90-30ad0bcfdbca";
            params.nic_mac_address = "00:00:00:00:00:00";
            params.target_compute_system_name = "test_vm";
            return wrapper.add_endpoint(params);
        },
        [](HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR configuration, HANDLE identity) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
            ASSERT_EQ(mock_operation_object, operation);
            const auto options_no_whitespace = trim_whitespace(configuration);
            const auto expected_options_no_whitespace = trim_whitespace(expected_modify_compute_system_configuration);
            ASSERT_STREQ(options_no_whitespace.c_str(), expected_options_no_whitespace.c_str());
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, remove_endpoint_from_compute_system_happy_path)
{
    constexpr auto expected_modify_compute_system_configuration = LR"(
        {
            "ResourcePath": "VirtualMachine/Devices/NetworkAdapters/{288cc1ac-8f31-4a09-9e90-30ad0bcfdbca}",
            "RequestType": "Remove"
        })";

    generic_operation_happy_path<decltype(HcsModifyComputeSystem)>(
        mock_api_table.ModifyComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(
                mpl::Level::debug,
                "remove_endpoint(...) > name: (test_vm), endpoint_guid: (288cc1ac-8f31-4a09-9e90-30ad0bcfdbca)");
            return wrapper.remove_endpoint("test_vm", "288cc1ac-8f31-4a09-9e90-30ad0bcfdbca");
        },
        [](HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR configuration, HANDLE identity) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
            ASSERT_EQ(mock_operation_object, operation);
            const auto options_no_whitespace = trim_whitespace(configuration);
            const auto expected_options_no_whitespace = trim_whitespace(expected_modify_compute_system_configuration);
            ASSERT_STREQ(options_no_whitespace.c_str(), expected_options_no_whitespace.c_str());
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, remove_endpoint_from_compute_system_hcs_open_fail)
{
    generic_operation_hcs_open_fail<decltype(HcsModifyComputeSystem)>(
        mock_api_table.ModifyComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "remove_endpoint(...)");
            return wrapper.remove_endpoint("test_vm", "288cc1ac-8f31-4a09-9e90-30ad0bcfdbca");
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, remove_endpoint_from_compute_system_create_operation_fail)
{
    generic_operation_create_operation_fail<decltype(HcsModifyComputeSystem)>(
        mock_api_table.ModifyComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "remove_endpoint(...)");
            return wrapper.remove_endpoint("test_vm", "288cc1ac-8f31-4a09-9e90-30ad0bcfdbca");
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, remove_endpoint_from_compute_system_fail)
{
    constexpr auto expected_modify_compute_system_configuration = LR"(
        {
            "ResourcePath": "VirtualMachine/Devices/NetworkAdapters/{288cc1ac-8f31-4a09-9e90-30ad0bcfdbca}",
            "RequestType": "Remove"
        })";

    generic_operation_fail<decltype(HcsModifyComputeSystem)>(
        mock_api_table.ModifyComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "remove_endpoint(...)");
            return wrapper.remove_endpoint("test_vm", "288cc1ac-8f31-4a09-9e90-30ad0bcfdbca");
        },
        [](HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR configuration, HANDLE identity) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
            ASSERT_EQ(mock_operation_object, operation);
            const auto options_no_whitespace = trim_whitespace(configuration);
            const auto expected_options_no_whitespace = trim_whitespace(expected_modify_compute_system_configuration);
            ASSERT_STREQ(options_no_whitespace.c_str(), expected_options_no_whitespace.c_str());
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, remove_endpoint_from_compute_system_wait_for_operation_result_fail)
{
    constexpr auto expected_modify_compute_system_configuration = LR"(
        {
            "ResourcePath": "VirtualMachine/Devices/NetworkAdapters/{288cc1ac-8f31-4a09-9e90-30ad0bcfdbca}",
            "RequestType": "Remove"
        })";

    generic_operation_wait_for_operation_fail<decltype(HcsModifyComputeSystem)>(
        mock_api_table.ModifyComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "remove_endpoint(...)");
            return wrapper.remove_endpoint("test_vm", "288cc1ac-8f31-4a09-9e90-30ad0bcfdbca");
        },
        [](HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR configuration, HANDLE identity) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
            ASSERT_EQ(mock_operation_object, operation);
            const auto options_no_whitespace = trim_whitespace(configuration);
            const auto expected_options_no_whitespace = trim_whitespace(expected_modify_compute_system_configuration);
            ASSERT_STREQ(options_no_whitespace.c_str(), expected_options_no_whitespace.c_str());
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, resize_memory_of_compute_system_happy_path)
{
    constexpr auto expected_modify_compute_system_configuration = LR"(
        {
            "ResourcePath": "VirtualMachine/ComputeTopology/Memory/SizeInMB",
            "RequestType": "Update",
            "Settings": 16384
        })";

    generic_operation_happy_path<decltype(HcsModifyComputeSystem)>(
        mock_api_table.ModifyComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug,
                                                 "resize_memory(...) > name: (test_vm), new_size_mb: (16384)");
            return wrapper.resize_memory("test_vm", 16384);
        },
        [](HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR configuration, HANDLE identity) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
            ASSERT_EQ(mock_operation_object, operation);
            const auto options_no_whitespace = trim_whitespace(configuration);
            const auto expected_options_no_whitespace = trim_whitespace(expected_modify_compute_system_configuration);
            ASSERT_STREQ(options_no_whitespace.c_str(), expected_options_no_whitespace.c_str());
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, resize_memory_of_compute_system_hcs_open_fail)
{
    generic_operation_hcs_open_fail<decltype(HcsModifyComputeSystem)>(
        mock_api_table.ModifyComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "resize_memory(...)");
            return wrapper.resize_memory("test_vm", 16384);
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, resize_memory_of_compute_system_create_operation_fail)
{
    generic_operation_create_operation_fail<decltype(HcsModifyComputeSystem)>(
        mock_api_table.ModifyComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "resize_memory(...)");
            return wrapper.resize_memory("test_vm", 16384);
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, resize_memory_of_compute_system_fail)
{
    constexpr auto expected_modify_compute_system_configuration = LR"(
        {
            "ResourcePath": "VirtualMachine/ComputeTopology/Memory/SizeInMB",
            "RequestType": "Update",
            "Settings": 16384
        })";

    generic_operation_fail<decltype(HcsModifyComputeSystem)>(
        mock_api_table.ModifyComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "resize_memory(...)");
            return wrapper.resize_memory("test_vm", 16384);
        },
        [](HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR configuration, HANDLE identity) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
            ASSERT_EQ(mock_operation_object, operation);
            const auto options_no_whitespace = trim_whitespace(configuration);
            const auto expected_options_no_whitespace = trim_whitespace(expected_modify_compute_system_configuration);
            ASSERT_STREQ(options_no_whitespace.c_str(), expected_options_no_whitespace.c_str());
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, resize_memory_of_compute_system_wait_for_operation_result_fail)
{
    constexpr auto expected_modify_compute_system_configuration = LR"(
        {
            "ResourcePath": "VirtualMachine/ComputeTopology/Memory/SizeInMB",
            "RequestType": "Update",
            "Settings": 16384
        })";

    generic_operation_wait_for_operation_fail<decltype(HcsModifyComputeSystem)>(
        mock_api_table.ModifyComputeSystem,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "resize_memory(...)");
            return wrapper.resize_memory("test_vm", 16384);
        },
        [](HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR configuration, HANDLE identity) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
            ASSERT_EQ(mock_operation_object, operation);
            const auto options_no_whitespace = trim_whitespace(configuration);
            const auto expected_options_no_whitespace = trim_whitespace(expected_modify_compute_system_configuration);
            ASSERT_STREQ(options_no_whitespace.c_str(), expected_options_no_whitespace.c_str());
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, get_compute_system_properties_happy_path)
{
    constexpr auto expected_vm_query = LR"(
        {
            "PropertyTypes":[]
        })";

    generic_operation_happy_path<decltype(HcsGetComputeSystemProperties)>(
        mock_api_table.GetComputeSystemProperties,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug,
                                                 "get_compute_system_properties(...) > name: (test_vm)");
            return wrapper.get_compute_system_properties("test_vm");
        },
        [](HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR propertyQuery) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
            ASSERT_EQ(mock_operation_object, operation);
            const auto options_no_whitespace = trim_whitespace(propertyQuery);
            const auto expected_options_no_whitespace = trim_whitespace(expected_vm_query);
            ASSERT_STREQ(options_no_whitespace.c_str(), expected_options_no_whitespace.c_str());
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, get_compute_system_properties_hcs_open_fail)
{
    generic_operation_hcs_open_fail<decltype(HcsGetComputeSystemProperties)>(
        mock_api_table.GetComputeSystemProperties,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "get_compute_system_properties(...)");
            return wrapper.get_compute_system_properties("test_vm");
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, get_compute_system_properties_create_operation_fail)
{
    generic_operation_create_operation_fail<decltype(HcsGetComputeSystemProperties)>(
        mock_api_table.GetComputeSystemProperties,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "get_compute_system_properties(...)");
            return wrapper.get_compute_system_properties("test_vm");
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, get_compute_system_properties_fail)
{
    constexpr auto expected_vm_query = LR"(
        {
            "PropertyTypes":[]
        })";

    generic_operation_fail<decltype(HcsGetComputeSystemProperties)>(
        mock_api_table.GetComputeSystemProperties,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "get_compute_system_properties(...)");
            return wrapper.get_compute_system_properties("test_vm");
        },
        [](HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR propertyQuery) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
            ASSERT_EQ(mock_operation_object, operation);
            const auto options_no_whitespace = trim_whitespace(propertyQuery);
            const auto expected_options_no_whitespace = trim_whitespace(expected_vm_query);
            ASSERT_STREQ(options_no_whitespace.c_str(), expected_options_no_whitespace.c_str());
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, get_compute_system_properties_wait_for_operation_result_fail)
{
    constexpr auto expected_vm_query = LR"(
        {
            "PropertyTypes":[]
        })";

    generic_operation_wait_for_operation_fail<decltype(HcsGetComputeSystemProperties)>(
        mock_api_table.GetComputeSystemProperties,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "get_compute_system_properties(...)");
            return wrapper.get_compute_system_properties("test_vm");
        },
        [](HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR propertyQuery) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
            ASSERT_EQ(mock_operation_object, operation);
            const auto options_no_whitespace = trim_whitespace(propertyQuery);
            const auto expected_options_no_whitespace = trim_whitespace(expected_vm_query);
            ASSERT_STREQ(options_no_whitespace.c_str(), expected_options_no_whitespace.c_str());
        });
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, get_compute_system_state_happy_path)
{
    static wchar_t result_doc[21] = L"{\"State\": \"Running\"}";
    static wchar_t expected_state[8] = L"Running";

    generic_operation_happy_path<decltype(HcsGetComputeSystemProperties)>(
        mock_api_table.GetComputeSystemProperties,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "get_compute_system_state(...) > name: (test_vm)");
            return wrapper.get_compute_system_state("test_vm");
        },
        [](HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR propertyQuery) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
            ASSERT_EQ(mock_operation_object, operation);
            ASSERT_EQ(propertyQuery, nullptr);
        },
        result_doc,
        expected_state);
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, get_compute_system_state_no_state)
{
    static wchar_t result_doc[21] = L"{\"Frodo\": \"Baggins\"}";
    static wchar_t expected_state[8] = L"Unknown";

    generic_operation_happy_path<decltype(HcsGetComputeSystemProperties)>(
        mock_api_table.GetComputeSystemProperties,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "get_compute_system_state(...)");
            return wrapper.get_compute_system_state("test_vm");
        },
        [](HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR propertyQuery) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
            ASSERT_EQ(mock_operation_object, operation);
            ASSERT_EQ(propertyQuery, nullptr);
        },
        result_doc,
        expected_state);
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, get_compute_system_state_hcs_open_fail)
{
    static wchar_t expected_status_msg[] = L"Unknown";
    generic_operation_hcs_open_fail<decltype(HcsGetComputeSystemProperties)>(
        mock_api_table.GetComputeSystemProperties,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "get_compute_system_state(...)");
            return wrapper.get_compute_system_state("test_vm");
        },
        expected_status_msg);
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, get_compute_system_state_create_operation_fail)
{
    static wchar_t expected_status_msg[] = L"Unknown";
    generic_operation_create_operation_fail<decltype(HcsGetComputeSystemProperties)>(
        mock_api_table.GetComputeSystemProperties,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "get_compute_system_state(...)");
            return wrapper.get_compute_system_state("test_vm");
        },
        expected_status_msg);
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, get_compute_system_state_fail)
{
    static wchar_t expected_status_msg[] = L"Unknown";

    generic_operation_fail<decltype(HcsGetComputeSystemProperties)>(
        mock_api_table.GetComputeSystemProperties,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "get_compute_system_state(...)");
            return wrapper.get_compute_system_state("test_vm");
        },
        [](HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR propertyQuery) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
            ASSERT_EQ(mock_operation_object, operation);
            ASSERT_EQ(nullptr, propertyQuery);
        },
        expected_status_msg);
}

// ---------------------------------------------------------

TEST_F(HyperVHCSAPI_UnitTests, get_compute_system_state_wait_for_operation_result_fail)
{
    static wchar_t expected_status_msg[] = L"Unknown";

    generic_operation_wait_for_operation_fail<decltype(HcsGetComputeSystemProperties)>(
        mock_api_table.GetComputeSystemProperties,
        [&](hyperv::hcs::HCSWrapper& wrapper) {
            logger_scope.mock_logger->expect_log(mpl::Level::debug, "get_compute_system_state(...)");
            return wrapper.get_compute_system_state("test_vm");
        },
        [](HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR propertyQuery) {
            ASSERT_EQ(mock_compute_system_object, computeSystem);
            ASSERT_EQ(mock_operation_object, operation);
            ASSERT_EQ(nullptr, propertyQuery);
        },
        nullptr,
        expected_status_msg);
}

} // namespace multipass::test
