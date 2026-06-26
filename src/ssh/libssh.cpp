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

#include <multipass/ssh/libssh.h>

extern "C"
{
int sftp_reply_version(sftp_client_message msg);
}

namespace mp = multipass;

mp::Libssh::Libssh(const Singleton<Libssh>::PrivatePass& pass) noexcept
    : Singleton<Libssh>::Singleton{pass}
{
}

// --- init / finalize --------------------------------------------------------
int mp::Libssh::ssh_init() const
{
    return ::ssh_init();
}

int mp::Libssh::ssh_finalize() const
{
    return ::ssh_finalize();
}

// --- session ----------------------------------------------------------------
ssh_session mp::Libssh::ssh_new() const
{
    return ::ssh_new();
}

void mp::Libssh::ssh_free(ssh_session session) const
{
    ::ssh_free(session);
}

void mp::Libssh::ssh_disconnect(ssh_session session) const
{
    ::ssh_disconnect(session);
}

int mp::Libssh::ssh_is_connected(ssh_session session) const
{
    return ::ssh_is_connected(session);
}

int mp::Libssh::ssh_options_set(ssh_session session,
                                enum ssh_options_e type,
                                const void* value) const
{
    return ::ssh_options_set(session, type, value);
}

const char* mp::Libssh::ssh_get_error(void* error) const
{
    return ::ssh_get_error(error);
}

socket_t mp::Libssh::ssh_get_fd(ssh_session session) const
{
    return ::ssh_get_fd(session);
}

int mp::Libssh::ssh_connect(ssh_session session) const
{
    return ::ssh_connect(session);
}

int mp::Libssh::ssh_userauth_publickey(ssh_session session,
                                       const char* username,
                                       const ssh_key privkey) const
{
    return ::ssh_userauth_publickey(session, username, privkey);
}

// --- channel ----------------------------------------------------------------
ssh_channel mp::Libssh::ssh_channel_new(ssh_session session) const
{
    return ::ssh_channel_new(session);
}

void mp::Libssh::ssh_channel_free(ssh_channel channel) const
{
    ::ssh_channel_free(channel);
}

int mp::Libssh::ssh_channel_is_closed(ssh_channel channel) const
{
    return ::ssh_channel_is_closed(channel);
}

int mp::Libssh::ssh_channel_is_eof(ssh_channel channel) const
{
    return ::ssh_channel_is_eof(channel);
}

int mp::Libssh::ssh_channel_is_open(ssh_channel channel) const
{
    return ::ssh_channel_is_open(channel);
}

int mp::Libssh::ssh_channel_close(ssh_channel channel) const
{
    return ::ssh_channel_close(channel);
}

ssh_session mp::Libssh::ssh_channel_get_session(ssh_channel channel) const
{
    return ::ssh_channel_get_session(channel);
}

int mp::Libssh::ssh_channel_read_timeout(ssh_channel channel,
                                         void* dest,
                                         uint32_t count,
                                         int is_stderr,
                                         int timeout_ms) const
{
    return ::ssh_channel_read_timeout(channel, dest, count, is_stderr, timeout_ms);
}

int mp::Libssh::ssh_channel_read_nonblocking(ssh_channel channel,
                                             void* dest,
                                             uint32_t count,
                                             int is_stderr) const
{
    return ::ssh_channel_read_nonblocking(channel, dest, count, is_stderr);
}

int mp::Libssh::ssh_channel_request_pty_size(ssh_channel channel,
                                             const char* term,
                                             int cols,
                                             int rows) const
{
    return ::ssh_channel_request_pty_size(channel, term, cols, rows);
}

int mp::Libssh::ssh_channel_change_pty_size(ssh_channel channel, int cols, int rows) const
{
    return ::ssh_channel_change_pty_size(channel, cols, rows);
}

int mp::Libssh::ssh_channel_write(ssh_channel channel, const void* data, uint32_t len) const
{
    return ::ssh_channel_write(channel, data, len);
}

int mp::Libssh::ssh_channel_open_session(ssh_channel channel) const
{
    return ::ssh_channel_open_session(channel);
}

int mp::Libssh::ssh_channel_request_exec(ssh_channel channel, const char* cmd) const
{
    return ::ssh_channel_request_exec(channel, cmd);
}

int mp::Libssh::ssh_channel_request_shell(ssh_channel channel) const
{
    return ::ssh_channel_request_shell(channel);
}

int mp::Libssh::ssh_channel_get_exit_state(ssh_channel channel,
                                           uint32_t* exit_code,
                                           char** exit_signal,
                                           int* core_dumped) const
{
    return ::ssh_channel_get_exit_state(channel, exit_code, exit_signal, core_dumped);
}

// --- channel callbacks ------------------------------------------------------
int mp::Libssh::ssh_add_channel_callbacks(ssh_channel channel, ssh_channel_callbacks cb) const
{
    return ::ssh_add_channel_callbacks(channel, cb);
}

int mp::Libssh::ssh_remove_channel_callbacks(ssh_channel channel, ssh_channel_callbacks cb) const
{
    return ::ssh_remove_channel_callbacks(channel, cb);
}

// --- event ------------------------------------------------------------------
ssh_event mp::Libssh::ssh_event_new() const
{
    return ::ssh_event_new();
}

void mp::Libssh::ssh_event_free(ssh_event event) const
{
    ::ssh_event_free(event);
}

int mp::Libssh::ssh_event_add_session(ssh_event event, ssh_session session) const
{
    return ::ssh_event_add_session(event, session);
}

int mp::Libssh::ssh_event_add_connector(ssh_event event, ssh_connector connector) const
{
    return ::ssh_event_add_connector(event, connector);
}

int mp::Libssh::ssh_event_remove_connector(ssh_event event, ssh_connector connector) const
{
    return ::ssh_event_remove_connector(event, connector);
}

int mp::Libssh::ssh_event_dopoll(ssh_event event, int timeout) const
{
    return ::ssh_event_dopoll(event, timeout);
}

// --- connector --------------------------------------------------------------
ssh_connector mp::Libssh::ssh_connector_new(ssh_session session) const
{
    return ::ssh_connector_new(session);
}

void mp::Libssh::ssh_connector_free(ssh_connector connector) const
{
    ::ssh_connector_free(connector);
}

int mp::Libssh::ssh_connector_set_in_channel(ssh_connector connector,
                                             ssh_channel channel,
                                             enum ssh_connector_flags_e flags) const
{
    return ::ssh_connector_set_in_channel(connector, channel, flags);
}

void mp::Libssh::ssh_connector_set_in_fd(ssh_connector connector, socket_t fd) const
{
    ::ssh_connector_set_in_fd(connector, fd);
}

int mp::Libssh::ssh_connector_set_out_channel(ssh_connector connector,
                                              ssh_channel channel,
                                              enum ssh_connector_flags_e flags) const
{
    return ::ssh_connector_set_out_channel(connector, channel, flags);
}

void mp::Libssh::ssh_connector_set_out_fd(ssh_connector connector, socket_t fd) const
{
    ::ssh_connector_set_out_fd(connector, fd);
}

// --- pki / keys -------------------------------------------------------------
int mp::Libssh::ssh_pki_generate(enum ssh_keytypes_e type, int parameter, ssh_key* pkey) const
{
    return ::ssh_pki_generate(type, parameter, pkey);
}

int mp::Libssh::ssh_pki_export_privkey_file(const ssh_key privkey,
                                            const char* passphrase,
                                            ssh_auth_callback auth_fn,
                                            void* auth_data,
                                            const char* filename) const
{
    return ::ssh_pki_export_privkey_file(privkey, passphrase, auth_fn, auth_data, filename);
}

int mp::Libssh::ssh_pki_import_privkey_file(const char* filename,
                                            const char* passphrase,
                                            ssh_auth_callback auth_fn,
                                            void* auth_data,
                                            ssh_key* pkey) const
{
    return ::ssh_pki_import_privkey_file(filename, passphrase, auth_fn, auth_data, pkey);
}

int mp::Libssh::ssh_pki_export_pubkey_base64(const ssh_key key, char** b64_key) const
{
    return ::ssh_pki_export_pubkey_base64(key, b64_key);
}

int mp::Libssh::ssh_pki_import_privkey_base64(const char* b64_key,
                                              const char* passphrase,
                                              ssh_auth_callback auth_fn,
                                              void* auth_data,
                                              ssh_key* pkey) const
{
    return ::ssh_pki_import_privkey_base64(b64_key, passphrase, auth_fn, auth_data, pkey);
}

void mp::Libssh::ssh_key_free(ssh_key key) const
{
    ::ssh_key_free(key);
}

// --- strings ----------------------------------------------------------------
void mp::Libssh::ssh_string_free(ssh_string str) const
{
    ::ssh_string_free(str);
}

void mp::Libssh::ssh_string_free_char(char* s) const
{
    ::ssh_string_free_char(s);
}

const char* mp::Libssh::ssh_string_get_char(ssh_string str) const
{
    return ::ssh_string_get_char(str);
}

size_t mp::Libssh::ssh_string_len(ssh_string str) const
{
    return ::ssh_string_len(str);
}

// --- sftp client ------------------------------------------------------------
sftp_session mp::Libssh::sftp_new(ssh_session session) const
{
    return ::sftp_new(session);
}

void mp::Libssh::sftp_free(sftp_session sftp) const
{
    ::sftp_free(sftp);
}

int mp::Libssh::sftp_init(sftp_session sftp) const
{
    return ::sftp_init(sftp);
}

sftp_file mp::Libssh::sftp_open(sftp_session session,
                                const char* file,
                                int accesstype,
                                mode_t mode) const
{
    return ::sftp_open(session, file, accesstype, mode);
}

int mp::Libssh::sftp_close(sftp_file file) const
{
    return ::sftp_close(file);
}

ssize_t mp::Libssh::sftp_read(sftp_file file, void* buf, size_t count) const
{
    return ::sftp_read(file, buf, count);
}

ssize_t mp::Libssh::sftp_write(sftp_file file, const void* buf, size_t count) const
{
    return ::sftp_write(file, buf, count);
}

int mp::Libssh::sftp_chmod(sftp_session sftp, const char* file, mode_t mode) const
{
    return ::sftp_chmod(sftp, file, mode);
}

int mp::Libssh::sftp_mkdir(sftp_session sftp, const char* directory, mode_t mode) const
{
    return ::sftp_mkdir(sftp, directory, mode);
}

int mp::Libssh::sftp_unlink(sftp_session sftp, const char* file) const
{
    return ::sftp_unlink(sftp, file);
}

int mp::Libssh::sftp_symlink(sftp_session sftp, const char* target, const char* dest) const
{
    return ::sftp_symlink(sftp, target, dest);
}

sftp_attributes mp::Libssh::sftp_stat(sftp_session session, const char* path) const
{
    return ::sftp_stat(session, path);
}

sftp_attributes mp::Libssh::sftp_lstat(sftp_session session, const char* path) const
{
    return ::sftp_lstat(session, path);
}

sftp_dir mp::Libssh::sftp_opendir(sftp_session session, const char* path) const
{
    return ::sftp_opendir(session, path);
}

int mp::Libssh::sftp_closedir(sftp_dir dir) const
{
    return ::sftp_closedir(dir);
}

sftp_attributes mp::Libssh::sftp_readdir(sftp_session session, sftp_dir dir) const
{
    return ::sftp_readdir(session, dir);
}

char* mp::Libssh::sftp_readlink(sftp_session sftp, const char* path) const
{
    return ::sftp_readlink(sftp, path);
}

int mp::Libssh::sftp_dir_eof(sftp_dir dir) const
{
    return ::sftp_dir_eof(dir);
}

int mp::Libssh::sftp_get_error(sftp_session sftp) const
{
    return ::sftp_get_error(sftp);
}

void mp::Libssh::sftp_attributes_free(sftp_attributes file) const
{
    ::sftp_attributes_free(file);
}

sftp_limits_t mp::Libssh::sftp_limits(sftp_session sftp) const
{
    return ::sftp_limits(sftp);
}

void mp::Libssh::sftp_limits_free(sftp_limits_t limits) const
{
    ::sftp_limits_free(limits);
}

// --- sftp server ------------------------------------------------------------
sftp_session mp::Libssh::sftp_server_new(ssh_session session, ssh_channel chan) const
{
    return ::sftp_server_new(session, chan);
}

void mp::Libssh::sftp_server_free(sftp_session sftp) const
{
    ::sftp_server_free(sftp);
}

void mp::Libssh::sftp_client_message_free(sftp_client_message msg) const
{
    ::sftp_client_message_free(msg);
}

void* mp::Libssh::sftp_handle(sftp_session sftp, ssh_string handle) const
{
    return ::sftp_handle(sftp, handle);
}

ssh_string mp::Libssh::sftp_handle_alloc(sftp_session sftp, void* info) const
{
    return ::sftp_handle_alloc(sftp, info);
}

void mp::Libssh::sftp_handle_remove(sftp_session sftp, void* handle) const
{
    ::sftp_handle_remove(sftp, handle);
}

sftp_client_message mp::Libssh::sftp_get_client_message(sftp_session sftp) const
{
    return ::sftp_get_client_message(sftp);
}

const char* mp::Libssh::sftp_client_message_get_data(sftp_client_message msg) const
{
    return ::sftp_client_message_get_data(msg);
}

const char* mp::Libssh::sftp_client_message_get_filename(sftp_client_message msg) const
{
    return ::sftp_client_message_get_filename(msg);
}

uint32_t mp::Libssh::sftp_client_message_get_flags(sftp_client_message msg) const
{
    return ::sftp_client_message_get_flags(msg);
}

const char* mp::Libssh::sftp_client_message_get_submessage(sftp_client_message msg) const
{
    return ::sftp_client_message_get_submessage(msg);
}

uint8_t mp::Libssh::sftp_client_message_get_type(sftp_client_message msg) const
{
    return ::sftp_client_message_get_type(msg);
}

int mp::Libssh::sftp_reply_attr(sftp_client_message msg, sftp_attributes attr) const
{
    return ::sftp_reply_attr(msg, attr);
}

int mp::Libssh::sftp_reply_data(sftp_client_message msg, const void* data, int len) const
{
    return ::sftp_reply_data(msg, data, len);
}

int mp::Libssh::sftp_reply_handle(sftp_client_message msg, ssh_string handle) const
{
    return ::sftp_reply_handle(msg, handle);
}

int mp::Libssh::sftp_reply_name(sftp_client_message msg,
                                const char* name,
                                sftp_attributes attr) const
{
    return ::sftp_reply_name(msg, name, attr);
}

int mp::Libssh::sftp_reply_names(sftp_client_message msg) const
{
    return ::sftp_reply_names(msg);
}

int mp::Libssh::sftp_reply_names_add(sftp_client_message msg,
                                     const char* file,
                                     const char* longname,
                                     sftp_attributes attr) const
{
    return ::sftp_reply_names_add(msg, file, longname, attr);
}

int mp::Libssh::sftp_reply_status(sftp_client_message msg,
                                  uint32_t status,
                                  const char* message) const
{
    return ::sftp_reply_status(msg, status, message);
}

int mp::Libssh::sftp_reply_version(sftp_client_message msg) const
{
    return ::sftp_reply_version(msg);
}
