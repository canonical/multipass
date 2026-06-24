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

#pragma once

#include "common.h"
#include "mock_singleton_helpers.h"

#include <multipass/ssh/libssh.h>

namespace multipass::test
{
class MockLibssh : public Libssh
{
public:
    using Libssh::Libssh;

    // --- init / finalize -----------------------------------------------------
    MOCK_METHOD(int, ssh_init, (), (const, override));
    MOCK_METHOD(int, ssh_finalize, (), (const, override));

    // --- session -------------------------------------------------------------
    MOCK_METHOD(ssh_session, ssh_new, (), (const, override));
    MOCK_METHOD(void, ssh_free, (ssh_session session), (const, override));
    MOCK_METHOD(void, ssh_disconnect, (ssh_session session), (const, override));
    MOCK_METHOD(int, ssh_is_connected, (ssh_session session), (const, override));
    MOCK_METHOD(int,
                ssh_options_set,
                (ssh_session session, enum ssh_options_e type, const void* value),
                (const, override));
    MOCK_METHOD(const char*, ssh_get_error, (void* error), (const, override));
    MOCK_METHOD(socket_t, ssh_get_fd, (ssh_session session), (const, override));
    MOCK_METHOD(int, ssh_connect, (ssh_session session), (const, override));
    MOCK_METHOD(int,
                ssh_userauth_publickey,
                (ssh_session session, const char* username, const ssh_key privkey),
                (const, override));

    // --- channel -------------------------------------------------------------
    MOCK_METHOD(ssh_channel, ssh_channel_new, (ssh_session session), (const, override));
    MOCK_METHOD(void, ssh_channel_free, (ssh_channel channel), (const, override));
    MOCK_METHOD(int, ssh_channel_is_closed, (ssh_channel channel), (const, override));
    MOCK_METHOD(int, ssh_channel_is_eof, (ssh_channel channel), (const, override));
    MOCK_METHOD(int, ssh_channel_is_open, (ssh_channel channel), (const, override));
    MOCK_METHOD(int, ssh_channel_close, (ssh_channel channel), (const, override));
    MOCK_METHOD(ssh_session, ssh_channel_get_session, (ssh_channel channel), (const, override));
    MOCK_METHOD(int,
                ssh_channel_read_timeout,
                (ssh_channel channel, void* dest, uint32_t count, int is_stderr, int timeout_ms),
                (const, override));
    MOCK_METHOD(int,
                ssh_channel_read_nonblocking,
                (ssh_channel channel, void* dest, uint32_t count, int is_stderr),
                (const, override));
    MOCK_METHOD(int,
                ssh_channel_request_pty_size,
                (ssh_channel channel, const char* term, int cols, int rows),
                (const, override));
    MOCK_METHOD(int,
                ssh_channel_change_pty_size,
                (ssh_channel channel, int cols, int rows),
                (const, override));
    MOCK_METHOD(int,
                ssh_channel_write,
                (ssh_channel channel, const void* data, uint32_t len),
                (const, override));
    MOCK_METHOD(int, ssh_channel_open_session, (ssh_channel channel), (const, override));
    MOCK_METHOD(int,
                ssh_channel_request_exec,
                (ssh_channel channel, const char* cmd),
                (const, override));
    MOCK_METHOD(int, ssh_channel_request_shell, (ssh_channel channel), (const, override));
    MOCK_METHOD(int,
                ssh_channel_get_exit_state,
                (ssh_channel channel, uint32_t* exit_code, char** exit_signal, int* core_dumped),
                (const, override));

    // --- channel callbacks ---------------------------------------------------
    MOCK_METHOD(int,
                ssh_add_channel_callbacks,
                (ssh_channel channel, ssh_channel_callbacks cb),
                (const, override));
    MOCK_METHOD(int,
                ssh_remove_channel_callbacks,
                (ssh_channel channel, ssh_channel_callbacks cb),
                (const, override));

    // --- event ---------------------------------------------------------------
    MOCK_METHOD(ssh_event, ssh_event_new, (), (const, override));
    MOCK_METHOD(void, ssh_event_free, (ssh_event event), (const, override));
    MOCK_METHOD(int,
                ssh_event_add_session,
                (ssh_event event, ssh_session session),
                (const, override));
    MOCK_METHOD(int,
                ssh_event_add_connector,
                (ssh_event event, ssh_connector connector),
                (const, override));
    MOCK_METHOD(int,
                ssh_event_remove_connector,
                (ssh_event event, ssh_connector connector),
                (const, override));
    MOCK_METHOD(int, ssh_event_dopoll, (ssh_event event, int timeout), (const, override));

    // --- connector -----------------------------------------------------------
    MOCK_METHOD(ssh_connector, ssh_connector_new, (ssh_session session), (const, override));
    MOCK_METHOD(void, ssh_connector_free, (ssh_connector connector), (const, override));
    MOCK_METHOD(int,
                ssh_connector_set_in_channel,
                (ssh_connector connector, ssh_channel channel, enum ssh_connector_flags_e flags),
                (const, override));
    MOCK_METHOD(void,
                ssh_connector_set_in_fd,
                (ssh_connector connector, socket_t fd),
                (const, override));
    MOCK_METHOD(int,
                ssh_connector_set_out_channel,
                (ssh_connector connector, ssh_channel channel, enum ssh_connector_flags_e flags),
                (const, override));
    MOCK_METHOD(void,
                ssh_connector_set_out_fd,
                (ssh_connector connector, socket_t fd),
                (const, override));

    // --- pki / keys ----------------------------------------------------------
    MOCK_METHOD(int,
                ssh_pki_generate,
                (enum ssh_keytypes_e type, int parameter, ssh_key* pkey),
                (const, override));
    MOCK_METHOD(int,
                ssh_pki_export_privkey_file,
                (const ssh_key privkey,
                 const char* passphrase,
                 ssh_auth_callback auth_fn,
                 void* auth_data,
                 const char* filename),
                (const, override));
    MOCK_METHOD(int,
                ssh_pki_import_privkey_file,
                (const char* filename,
                 const char* passphrase,
                 ssh_auth_callback auth_fn,
                 void* auth_data,
                 ssh_key* pkey),
                (const, override));
    MOCK_METHOD(int,
                ssh_pki_export_pubkey_base64,
                (const ssh_key key, char** b64_key),
                (const, override));
    MOCK_METHOD(int,
                ssh_pki_import_privkey_base64,
                (const char* b64_key,
                 const char* passphrase,
                 ssh_auth_callback auth_fn,
                 void* auth_data,
                 ssh_key* pkey),
                (const, override));
    MOCK_METHOD(void, ssh_key_free, (ssh_key key), (const, override));

    // --- strings -------------------------------------------------------------
    MOCK_METHOD(void, ssh_string_free, (ssh_string str), (const, override));
    MOCK_METHOD(void, ssh_string_free_char, (char* s), (const, override));
    MOCK_METHOD(const char*, ssh_string_get_char, (ssh_string str), (const, override));
    MOCK_METHOD(size_t, ssh_string_len, (ssh_string str), (const, override));

    // --- sftp client ---------------------------------------------------------
    MOCK_METHOD(sftp_session, sftp_new, (ssh_session session), (const, override));
    MOCK_METHOD(void, sftp_free, (sftp_session sftp), (const, override));
    MOCK_METHOD(int, sftp_init, (sftp_session sftp), (const, override));
    MOCK_METHOD(sftp_file,
                sftp_open,
                (sftp_session session, const char* file, int accesstype, mode_t mode),
                (const, override));
    MOCK_METHOD(int, sftp_close, (sftp_file file), (const, override));
    MOCK_METHOD(ssize_t, sftp_read, (sftp_file file, void* buf, size_t count), (const, override));
    MOCK_METHOD(ssize_t,
                sftp_write,
                (sftp_file file, const void* buf, size_t count),
                (const, override));
    MOCK_METHOD(int,
                sftp_chmod,
                (sftp_session sftp, const char* file, mode_t mode),
                (const, override));
    MOCK_METHOD(int,
                sftp_mkdir,
                (sftp_session sftp, const char* directory, mode_t mode),
                (const, override));
    MOCK_METHOD(int, sftp_unlink, (sftp_session sftp, const char* file), (const, override));
    MOCK_METHOD(int,
                sftp_symlink,
                (sftp_session sftp, const char* target, const char* dest),
                (const, override));
    MOCK_METHOD(sftp_attributes,
                sftp_stat,
                (sftp_session session, const char* path),
                (const, override));
    MOCK_METHOD(sftp_attributes,
                sftp_lstat,
                (sftp_session session, const char* path),
                (const, override));
    MOCK_METHOD(sftp_dir,
                sftp_opendir,
                (sftp_session session, const char* path),
                (const, override));
    MOCK_METHOD(int, sftp_closedir, (sftp_dir dir), (const, override));
    MOCK_METHOD(sftp_attributes,
                sftp_readdir,
                (sftp_session session, sftp_dir dir),
                (const, override));
    MOCK_METHOD(char*, sftp_readlink, (sftp_session sftp, const char* path), (const, override));
    MOCK_METHOD(int, sftp_dir_eof, (sftp_dir dir), (const, override));
    MOCK_METHOD(int, sftp_get_error, (sftp_session sftp), (const, override));
    MOCK_METHOD(void, sftp_attributes_free, (sftp_attributes file), (const, override));
    MOCK_METHOD(sftp_limits_t, sftp_limits, (sftp_session sftp), (const, override));
    MOCK_METHOD(void, sftp_limits_free, (sftp_limits_t limits), (const, override));

    // --- sftp server ---------------------------------------------------------
    MOCK_METHOD(sftp_session,
                sftp_server_new,
                (ssh_session session, ssh_channel chan),
                (const, override));
    MOCK_METHOD(void, sftp_server_free, (sftp_session sftp), (const, override));
    MOCK_METHOD(void, sftp_client_message_free, (sftp_client_message msg), (const, override));
    MOCK_METHOD(void*, sftp_handle, (sftp_session sftp, ssh_string handle), (const, override));
    MOCK_METHOD(ssh_string, sftp_handle_alloc, (sftp_session sftp, void* info), (const, override));
    MOCK_METHOD(void, sftp_handle_remove, (sftp_session sftp, void* handle), (const, override));
    MOCK_METHOD(sftp_client_message,
                sftp_get_client_message,
                (sftp_session sftp),
                (const, override));
    MOCK_METHOD(const char*,
                sftp_client_message_get_data,
                (sftp_client_message msg),
                (const, override));
    MOCK_METHOD(const char*,
                sftp_client_message_get_filename,
                (sftp_client_message msg),
                (const, override));
    MOCK_METHOD(uint32_t,
                sftp_client_message_get_flags,
                (sftp_client_message msg),
                (const, override));
    MOCK_METHOD(const char*,
                sftp_client_message_get_submessage,
                (sftp_client_message msg),
                (const, override));
    MOCK_METHOD(uint8_t,
                sftp_client_message_get_type,
                (sftp_client_message msg),
                (const, override));
    MOCK_METHOD(int,
                sftp_reply_attr,
                (sftp_client_message msg, sftp_attributes attr),
                (const, override));
    MOCK_METHOD(int,
                sftp_reply_data,
                (sftp_client_message msg, const void* data, int len),
                (const, override));
    MOCK_METHOD(int,
                sftp_reply_handle,
                (sftp_client_message msg, ssh_string handle),
                (const, override));
    MOCK_METHOD(int,
                sftp_reply_name,
                (sftp_client_message msg, const char* name, sftp_attributes attr),
                (const, override));
    MOCK_METHOD(int, sftp_reply_names, (sftp_client_message msg), (const, override));
    MOCK_METHOD(
        int,
        sftp_reply_names_add,
        (sftp_client_message msg, const char* file, const char* longname, sftp_attributes attr),
        (const, override));
    MOCK_METHOD(int,
                sftp_reply_status,
                (sftp_client_message msg, uint32_t status, const char* message),
                (const, override));
    MOCK_METHOD(int, sftp_reply_version, (sftp_client_message msg), (const, override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockLibssh, Libssh);
};
} // namespace multipass::test
