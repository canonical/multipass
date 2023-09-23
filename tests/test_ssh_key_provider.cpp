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

#include "common.h"
#include "file_operations.h"
#include "mock_utils.h"
#include "temp_dir.h"

#include <multipass/ssh/openssh_key_provider.h>
#include <multipass/utils.h>

#include <thread>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

struct SSHKeyProvider : public testing::Test
{
    mpt::TempDir key_dir;
};

TEST_F(SSHKeyProvider, creates_key)
{
    mp::OpenSSHKeyProvider key_provider{key_dir.path()};

    EXPECT_THAT(key_provider.public_key_as_base64(), StrNe(""));
}

TEST_F(SSHKeyProvider, imports_existing_key)
{
    constexpr auto key_data = "-----BEGIN RSA PRIVATE KEY-----\n"
                              "MIIEpAIBAAKCAQEAv3lEFtxT3kd2OrWQ8k3v1SHILNDwwm9U7awNbLDqVEresZNd\n"
                              "mRGmH381fO8tHpNdeQ+XEMff16FZiMKRQWx5OlHTQ33cS7X/huZ5Ge5YVKsBMmqy\n"
                              "vJADK7Ud38mNaKi3hqepD87labVmY1ARTNHSLDG68c5bIyquvzbawwwkM7qPTbw5\n"
                              "ZHOEpKehwPZ/034oPnmPV3BAbX1HySi/zrOZE/D1JW3uHvhF1yprDWhJumOVBSYB\n"
                              "zDloJSsFfFEEYOzkdmAwZ0q3yfMVxLiwTlP2Hhf+i8kOzjQfz29PPfNwroYJZqKT\n"
                              "Eg8YAqBr28ryHzHa8W+htUoZbntID2w9aDeJ2wIDAQABAoIBABpk0vf7wyve2fNZ\n"
                              "1/MuvyK4F2nmG2oSArkIgIk9EfAwqeX8lGhnQGkTFgJ0zdlrIvVvKrnLc5W7ziXF\n"
                              "/FPyafuaD+87yEQ/gEvONV9XtaFmOTID90N67pT10HpqxC1rJHFRZ0KgmIsr0ENc\n"
                              "ZCYcvkYNTOHMOk/ssE33d8xvPgZLMf/EvVqcgPyhJXXg0Y1po9cLPQjCBCfmigiM\n"
                              "U+3Hlrz8w6rtp3RUuM2jzrA+uHQGSa4fC/Wn2WT5fR2RZz7BPdJ+kHlTfFRq27oy\n"
                              "lsTITYDJf26ej1wmsWIV4AqznV33xSRZBK6KZjq98D6MKc28fyfZQKxnc1jWG1Xi\n"
                              "erLM+YECgYEA73wVxCdX/9eBXlquQ5wrj94UFOV9zlbr/oE0CYZC8SkG6fCf9/a/\n"
                              "lUUz25CK+kugJcOFHOmOXIjydXzDDFXEUgsf6MeX0WiLy6rvFdlq3xnQ9oUKBzCv\n"
                              "6gLL9s2Ozo4NMeY3rlqxAswdyGx3f5HHkB722MeUlafjXPkJ82m61GECgYEAzK2V\n"
                              "iX1v+b76HG9XnDd1eK0L/JINSJrFJUMD1KhY84FmXqPoBWcuoEtUPA+cvOhAzr3l\n"
                              "TFqKbXbY5JVx1c66uEkCMYYVPYyOVZNnEz+bGOmrK2NaYDwIySG6WhD2Zh69VIXa\n"
                              "kfvMzba0M26FXjWBDN3oluT6RLBHb2xdZgMHx7sCgYB1B3QziO5t7cggbbve+kAn\n"
                              "a+TwWT1jSgLFOipNxTiNVPk19QqXSBNTRKAU2cuwiKhYC/XOrSuOeLXTSAagzoDD\n"
                              "fwA25uJ/yNEX1A5F5RteruT4swa1gMtWVcuKbeUtdylnixMGtvbtYQXk3WyAAKM/\n"
                              "AIKsaMtpXsOyuVhthOtxwQKBgQCBvIGtzcHdd01IGtdYoNqoLGANr3IWFGxkSw8x\n"
                              "i6geaWY/FPvr+NRYLIdvLqI2J610nm+qrzVRX2Tpt0SZttkqGLT4OTpbci2CVtWe\n"
                              "INIpv2uNLAPMPiF/hA6AKoJUhqWR3uqFYCsYNfgRJbwJ1DZBtqNIikmMooQVP4YQ\n"
                              "NFmJIwKBgQCjxMF4SFzzRbNfiHKLL39D8RHlCPalbmX2CXaiUT4H1rq2oK3EiI+O\n"
                              "+SzzmxbHAjFRRuKeqhmC9+yhhHssBt6lJe71Fl3e01McjOcW9P1AZQdgYsDyCqR0\n"
                              "Yy460TKDO1em0N9GlXfsYgiSFJv1WmD7M/kvGpGxSERcnR4+bBd2BQ==\n"
                              "-----END RSA PRIVATE KEY-----\n";

    auto ssh_keys_path = MP_UTILS.make_dir(key_dir.path(), "ssh-keys");
    mpt::make_file_with_content(QDir{ssh_keys_path}.filePath("id_rsa"), key_data);

    mp::OpenSSHKeyProvider key_provider{key_dir.path()};
    EXPECT_THAT(key_provider.private_key_as_base64(), StrEq(key_data));

    constexpr auto expected_pub_key_data =
        "AAAAB3NzaC1yc2EAAAADAQABAAABAQC/eUQW3FPeR3Y6tZDyTe/VIcgs0PDCb1TtrA1ssOpUSt6xk12ZEaYffzV87y0ek115D5cQx9/"
        "XoVmIwpFBbHk6UdNDfdxLtf+G5nkZ7lhUqwEyarK8kAMrtR3fyY1oqLeGp6kPzuVptWZjUBFM0dIsMbrxzlsjKq6/"
        "NtrDDCQzuo9NvDlkc4Skp6HA9n/Tfig+eY9XcEBtfUfJKL/"
        "Os5kT8PUlbe4e+EXXKmsNaEm6Y5UFJgHMOWglKwV8UQRg7OR2YDBnSrfJ8xXEuLBOU/YeF/6LyQ7ONB/"
        "Pb08983CuhglmopMSDxgCoGvbyvIfMdrxb6G1Shlue0gPbD1oN4nb";

    EXPECT_THAT(key_provider.public_key_as_base64(), StrEq(expected_pub_key_data));
}

TEST_F(SSHKeyProvider, public_key_is_stable)
{
    mp::OpenSSHKeyProvider key_provider{key_dir.path()};

    const auto key_one = key_provider.public_key_as_base64();
    const auto key_two = key_provider.public_key_as_base64();

    EXPECT_THAT(key_one, StrEq(key_two));
}
