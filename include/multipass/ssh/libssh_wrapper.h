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

#include <multipass/singleton.h>

#include <libssh/callbacks.h>
#include <libssh/libssh.h>
#include <libssh/sftp.h>

#define MP_LIBSSH multipass::Libssh::instance()

namespace multipass
{
class Libssh : public Singleton<Libssh>
{
public:
    Libssh(const Singleton<Libssh>::PrivatePass&) noexcept;

    // --- init / finalize -----------------------------------------------------
    virtual int ssh_init() const;
    virtual int ssh_finalize() const;

    // --- session -------------------------------------------------------------
    virtual ssh_session ssh_new() const;
    virtual void ssh_free(ssh_session session) const;
    virtual void ssh_disconnect(ssh_session session) const;
    virtual int ssh_is_connected(ssh_session session) const;
    virtual int ssh_options_set(ssh_session session,
                                enum ssh_options_e type,
                                const void* value) const;
    virtual const char* ssh_get_error(void* error) const;
    virtual socket_t ssh_get_fd(ssh_session session) const;
    virtual int ssh_connect(ssh_session session) const;
    virtual int ssh_userauth_publickey(ssh_session session,
                                       const char* username,
                                       const ssh_key privkey) const;

    // --- channel -------------------------------------------------------------
    virtual ssh_channel ssh_channel_new(ssh_session session) const;
    virtual void ssh_channel_free(ssh_channel channel) const;
    virtual int ssh_channel_is_closed(ssh_channel channel) const;
    virtual int ssh_channel_is_eof(ssh_channel channel) const;
    virtual int ssh_channel_is_open(ssh_channel channel) const;
    virtual int ssh_channel_close(ssh_channel channel) const;
    virtual ssh_session ssh_channel_get_session(ssh_channel channel) const;
    virtual int ssh_channel_read_timeout(ssh_channel channel,
                                         void* dest,
                                         uint32_t count,
                                         int is_stderr,
                                         int timeout_ms) const;
    virtual int ssh_channel_read_nonblocking(ssh_channel channel,
                                             void* dest,
                                             uint32_t count,
                                             int is_stderr) const;
    virtual int ssh_channel_poll_timeout(ssh_channel channel, int timeout, int is_stderr) const;
    virtual int ssh_channel_request_pty_size(ssh_channel channel,
                                             const char* term,
                                             int cols,
                                             int rows) const;
    virtual int ssh_channel_change_pty_size(ssh_channel channel, int cols, int rows) const;
    virtual int ssh_channel_write(ssh_channel channel, const void* data, uint32_t len) const;
    virtual int ssh_channel_open_session(ssh_channel channel) const;
    virtual int ssh_channel_request_exec(ssh_channel channel, const char* cmd) const;
    virtual int ssh_channel_request_shell(ssh_channel channel) const;
    virtual int ssh_channel_get_exit_state(ssh_channel channel,
                                           uint32_t* exit_code,
                                           char** exit_signal,
                                           int* core_dumped) const;

    // --- channel callbacks ---------------------------------------------------
    virtual int ssh_add_channel_callbacks(ssh_channel channel, ssh_channel_callbacks cb) const;
    virtual int ssh_remove_channel_callbacks(ssh_channel channel, ssh_channel_callbacks cb) const;

    // --- event ---------------------------------------------------------------
    virtual ssh_event ssh_event_new() const;
    virtual void ssh_event_free(ssh_event event) const;
    virtual int ssh_event_add_session(ssh_event event, ssh_session session) const;
    virtual int ssh_event_add_connector(ssh_event event, ssh_connector connector) const;
    virtual int ssh_event_remove_connector(ssh_event event, ssh_connector connector) const;
    virtual int ssh_event_dopoll(ssh_event event, int timeout) const;

    // --- connector -----------------------------------------------------------
    virtual ssh_connector ssh_connector_new(ssh_session session) const;
    virtual void ssh_connector_free(ssh_connector connector) const;
    virtual int ssh_connector_set_in_channel(ssh_connector connector,
                                             ssh_channel channel,
                                             enum ssh_connector_flags_e flags) const;
    virtual void ssh_connector_set_in_fd(ssh_connector connector, socket_t fd) const;
    virtual int ssh_connector_set_out_channel(ssh_connector connector,
                                              ssh_channel channel,
                                              enum ssh_connector_flags_e flags) const;
    virtual void ssh_connector_set_out_fd(ssh_connector connector, socket_t fd) const;

    // --- pki / keys ----------------------------------------------------------
    virtual int ssh_pki_generate(enum ssh_keytypes_e type, int parameter, ssh_key* pkey) const;
    virtual int ssh_pki_export_privkey_file(const ssh_key privkey,
                                            const char* passphrase,
                                            ssh_auth_callback auth_fn,
                                            void* auth_data,
                                            const char* filename) const;
    virtual int ssh_pki_import_privkey_file(const char* filename,
                                            const char* passphrase,
                                            ssh_auth_callback auth_fn,
                                            void* auth_data,
                                            ssh_key* pkey) const;
    virtual int ssh_pki_export_pubkey_base64(const ssh_key key, char** b64_key) const;
    virtual int ssh_pki_import_privkey_base64(const char* b64_key,
                                              const char* passphrase,
                                              ssh_auth_callback auth_fn,
                                              void* auth_data,
                                              ssh_key* pkey) const;
    virtual void ssh_key_free(ssh_key key) const;

    // --- strings -------------------------------------------------------------
    virtual void ssh_string_free(ssh_string str) const;
    virtual void ssh_string_free_char(char* s) const;
    virtual const char* ssh_string_get_char(ssh_string str) const;
    virtual size_t ssh_string_len(ssh_string str) const;

    // --- sftp client ---------------------------------------------------------
    virtual sftp_session sftp_new(ssh_session session) const;
    virtual void sftp_free(sftp_session sftp) const;
    virtual int sftp_init(sftp_session sftp) const;
    virtual sftp_file sftp_open(sftp_session session,
                                const char* file,
                                int accesstype,
                                mode_t mode) const;
    virtual int sftp_close(sftp_file file) const;
    virtual ssize_t sftp_read(sftp_file file, void* buf, size_t count) const;
    virtual ssize_t sftp_write(sftp_file file, const void* buf, size_t count) const;
    virtual int sftp_chmod(sftp_session sftp, const char* file, mode_t mode) const;
    virtual int sftp_mkdir(sftp_session sftp, const char* directory, mode_t mode) const;
    virtual int sftp_unlink(sftp_session sftp, const char* file) const;
    virtual int sftp_symlink(sftp_session sftp, const char* target, const char* dest) const;
    virtual sftp_attributes sftp_stat(sftp_session session, const char* path) const;
    virtual sftp_attributes sftp_lstat(sftp_session session, const char* path) const;
    virtual sftp_dir sftp_opendir(sftp_session session, const char* path) const;
    virtual int sftp_closedir(sftp_dir dir) const;
    virtual sftp_attributes sftp_readdir(sftp_session session, sftp_dir dir) const;
    virtual char* sftp_readlink(sftp_session sftp, const char* path) const;
    virtual int sftp_dir_eof(sftp_dir dir) const;
    virtual int sftp_get_error(sftp_session sftp) const;
    virtual void sftp_attributes_free(sftp_attributes file) const;
    virtual sftp_limits_t sftp_limits(sftp_session sftp) const;
    // libssh >= 0.11
    virtual void sftp_limits_free(sftp_limits_t limits) const;

    // --- sftp server ---------------------------------------------------------
    virtual sftp_session sftp_server_new(ssh_session session, ssh_channel chan) const;
    virtual void sftp_server_free(sftp_session sftp) const;
    virtual void sftp_client_message_free(sftp_client_message msg) const;
    virtual void* sftp_handle(sftp_session sftp, ssh_string handle) const;
    virtual ssh_string sftp_handle_alloc(sftp_session sftp, void* info) const;
    virtual void sftp_handle_remove(sftp_session sftp, void* handle) const;
    virtual sftp_client_message sftp_get_client_message(sftp_session sftp) const;
    virtual const char* sftp_client_message_get_data(sftp_client_message msg) const;
    virtual const char* sftp_client_message_get_filename(sftp_client_message msg) const;
    virtual uint32_t sftp_client_message_get_flags(sftp_client_message msg) const;
    virtual const char* sftp_client_message_get_submessage(sftp_client_message msg) const;
    virtual uint8_t sftp_client_message_get_type(sftp_client_message msg) const;
    virtual int sftp_reply_attr(sftp_client_message msg, sftp_attributes attr) const;
    virtual int sftp_reply_data(sftp_client_message msg, const void* data, int len) const;
    virtual int sftp_reply_handle(sftp_client_message msg, ssh_string handle) const;
    virtual int sftp_reply_name(sftp_client_message msg,
                                const char* name,
                                sftp_attributes attr) const;
    virtual int sftp_reply_names(sftp_client_message msg) const;
    virtual int sftp_reply_names_add(sftp_client_message msg,
                                     const char* file,
                                     const char* longname,
                                     sftp_attributes attr) const;
    virtual int sftp_reply_status(sftp_client_message msg,
                                  uint32_t status,
                                  const char* message) const;
    // Not exposed in libssh's public headers; forward-declared with C linkage and
    // implemented by libssh.
    virtual int sftp_reply_version(sftp_client_message msg) const;
};
} // namespace multipass
