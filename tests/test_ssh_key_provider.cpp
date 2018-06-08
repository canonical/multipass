/*
 * Copyright (C) 2017 Canonical, Ltd.
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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include <multipass/ssh/openssh_key_provider.h>

#include <QTemporaryDir>
#include <gmock/gmock.h>

#include <chrono>
#include <thread>

namespace mp = multipass;
using namespace testing;

struct SSHKeyProvider : public testing::Test
{
    SSHKeyProvider()
    {
        if (!key_dir.isValid())
            throw std::runtime_error("test failed to create temp directory");
    }
    QTemporaryDir key_dir;
};

TEST_F(SSHKeyProvider, creates_key)
{
    mp::OpenSSHKeyProvider key_provider{key_dir.path(), QDir(key_dir.path()).filePath(("fallback"))};

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

    constexpr auto key_dir_name = "ssh-keys";
    QDir dir{key_dir.path()};
    auto made_path = dir.mkpath(key_dir_name);
    if (!made_path)
        throw std::runtime_error("test failed to create temporary key directory");

    dir.cd(key_dir_name);
    QFile key_file{dir.filePath("id_rsa")};
    auto opened = key_file.open(QIODevice::WriteOnly);
    if (!opened)
        throw std::runtime_error("test unable to open file for writing");

    auto written = key_file.write(key_data);
    if (written == -1)
        throw std::runtime_error("test unable to write key data");

    key_file.close();

    mp::OpenSSHKeyProvider key_provider{key_dir.path(), QDir(key_dir.path()).filePath("fallback")};
    EXPECT_THAT(key_provider.private_key_as_base64(), StrEq(key_data));

    constexpr auto expected_pub_key_data =
        "AAAAB3NzaC1yc2EAAAADAQABAAABAQC/eUQW3FPeR3Y6tZDyTe/VIcgs0PDCb1TtrA1ssOpUSt6xk12ZEaYffzV87y0ek115D5cQx9/"
        "XoVmIwpFBbHk6UdNDfdxLtf+G5nkZ7lhUqwEyarK8kAMrtR3fyY1oqLeGp6kPzuVptWZjUBFM0dIsMbrxzlsjKq6/"
        "NtrDDCQzuo9NvDlkc4Skp6HA9n/Tfig+eY9XcEBtfUfJKL/"
        "Os5kT8PUlbe4e+EXXKmsNaEm6Y5UFJgHMOWglKwV8UQRg7OR2YDBnSrfJ8xXEuLBOU/YeF/6LyQ7ONB/"
        "Pb08983CuhglmopMSDxgCoGvbyvIfMdrxb6G1Shlue0gPbD1oN4nb";

    EXPECT_THAT(key_provider.public_key_as_base64(), StrEq(expected_pub_key_data));
}

TEST_F(SSHKeyProvider, imports_fallback_key)
{
    constexpr auto key_data = "-----BEGIN RSA PRIVATE KEY-----\n"
                              "MIIEpgIBAAKCAQEA5dgJzMDBi4uTekzYo4xey57EOqCAlERWCs2sP57/jL15x1m2\n"
                              "ZJMED3RB10rhLY6wlAkKSe3a+OYErrQ12n7cMEBCZt1BM98J1U7RZf1eWcJxv+GP\n"
                              "m2Y42uDwQ0rwoqederb7Xj/Qs56fzA2nd1+rq82mdoC5U5Oojn6fd7SvAF7lcWG4\n"
                              "IlXAyo4MNIxx0w8H6FtsDDxk3Op602bFJC41E1GA/sTBH3sd+XtRmL71m805RqJ0\n"
                              "cSA6Ud1aBPGQ/GguZbWZvAgDXPWcCxosd/A1HTfp60sCfIXsDaHyXD/E3O/NbT4C\n"
                              "w1ICyjcdhSvgt1CK+NYJUi4N0LE6ihbnGWt+DwIDAQABAoIBAQClCjsHNoYDwMa2\n"
                              "NAX/ciUDi06WVnfdfzBVc5gy2uXfVpLiTY4Fq/dnbXB1uoP0pZgvxEoKCeyJewlz\n"
                              "tV1SNnO/hfToTgmqyIYUEBOWa5fbVh89zCbFTGKY8Jo1h9NAyKDw5AGdjK88uzM9\n"
                              "X7SXqC/qHrJgN1BcLhQDtfKmDyz2UtFECg45nT4gdXfjuXulbh/CA5ocEWO2tx6s\n"
                              "DbdKiCXLyGXDBp3iUrKZgv3y1aEuqu7a0gLXwuWXvsieSr1K7eDoUozpSFRJ402h\n"
                              "/eKdNwAI3480Yfkq+W1+In7o73a30ArZsvPjAV301wjYKeipJqJ2B8TxfIK/nmOc\n"
                              "7iMBAJpZAoGBAPyC3Bm0H8tJjP/fIOoUrdyQI5ieFjLwdJCtHF0ENm03e8VCDX7S\n"
                              "Pe2vxxmwuOhmmjXZsoyloupBXs88IFA9Ub9NEiU7HeaWQZij8yxAAsk3pSPk5PII\n"
                              "B5fc2IhqZVtDrmy9SFxoG2z3lLvos424JxybWiRZiRvJ1H5CznDMxCTlAoGBAOkF\n"
                              "AO5OuiFRs0KQU9p7FTRUqIqmjFZWnKD95Y2WstD4mFLq1uHQKOtwOgiwMgc+dR3+\n"
                              "6KlGQUOrLZZ64Hnqv3tzCDq8wicUDRAb77YHCPURCRkdlTnU8k6DfEOJQE+F+NIx\n"
                              "pPHNuTGk+gKcnOBasX4YDsTBGueXm8UceirlJjvjAoGBAOcglHxGfi+h9Bik0MjS\n"
                              "As33dU2VrfYUWG2wtlyBpZvUhErrCdVCdYLhORnPw4CdoZuW1zKbzgIjsbd4bo9j\n"
                              "gTbcQbaoZNcSQ5qGvueK88HgWB0Bv3m2VdTwRZaf5IRj7/0CSICkni3gdFiHSAp5\n"
                              "O/uYSysodTJlTEYBulpPY13tAoGBAJ9H+lDro7Ou4zg26/rGc8IntGXCXt+NqNcr\n"
                              "dsSIJIdPxt+3A1esMKxvNFxIBIqLGQd9/Y/oJCibVDgumk78NhOumAt+h8CqphtC\n"
                              "3WKyAF90W6G9ZQn5FACzRDfuE/XIAEO+B5/6HgNfYD50y49Yr0cgLsgzr65rJeFh\n"
                              "C+lSZgY9AoGBAJO2LnRuJOo5+ETQKh6q5GJhz+qnWEeElg08SSCxKYm/gLlOBP3i\n"
                              "hYqjMRF9ObdNTo8f5OG6rUGOq0zi5YV5oNeQ98MlGBtGvrXGZ6f0cThITd2WUAqb\n"
                              "9czYoU8PEyj1rApS01C24xctx9+31R8pQS20yZ9owYp/nDQOKyP+HWIJ\n"
                              "-----END RSA PRIVATE KEY-----\n";

    constexpr auto key_dir_name = "ssh-keys";
    constexpr auto fallback_dir_name = "fallback/ssh-keys";
    QDir dir{key_dir.path()};
    auto made_path = dir.mkpath(key_dir_name);
    if (!made_path)
        throw std::runtime_error("test failed to create temporary key directory");
    made_path = dir.mkpath(fallback_dir_name);
    if (!made_path)
        throw std::runtime_error("test failed to create temporary fallback directory");

    dir.cd(fallback_dir_name);
    QFile key_file{dir.filePath("id_rsa")};
    auto opened = key_file.open(QIODevice::WriteOnly);
    if (!opened)
        throw std::runtime_error("test unable to open file for writing");

    auto written = key_file.write(key_data);
    if (written == -1)
        throw std::runtime_error("test unable to write key data");

    key_file.close();

    EXPECT_FALSE(QFile::exists(QDir(key_dir.path()).filePath("ssh-keys/id_rsa")));

    mp::OpenSSHKeyProvider key_provider{key_dir.path(), QDir(key_dir.path()).filePath("fallback")};
    EXPECT_TRUE(QFile::exists(QDir(key_dir.path()).filePath("ssh-keys/id_rsa")));
    EXPECT_THAT(key_provider.private_key_as_base64(), StrEq(key_data));

    constexpr auto expected_pub_key_data =
        "AAAAB3NzaC1yc2EAAAADAQABAAABAQDl2AnMwMGLi5N6TNijjF7LnsQ6oICURFYKzaw/nv+MvXnHWbZkkwQPdEHXSuEtjrCUCQpJ7dr"
        "45gSutDXaftwwQEJm3UEz3wnVTtFl/V5ZwnG/4Y+bZjja4PBDSvCip516tvteP9Cznp/MDad3X6urzaZ2gLlTk6iOfp93tK8AXuVxYb"
        "giVcDKjgw0jHHTDwfoW2wMPGTc6nrTZsUkLjUTUYD+xMEfex35e1GYvvWbzTlGonRxIDpR3VoE8ZD8aC5ltZm8CANc9ZwLGix38DUdN"
        "+nrSwJ8hewNofJcP8Tc781tPgLDUgLKNx2FK+C3UIr41glSLg3QsTqKFucZa34P";

    EXPECT_THAT(key_provider.public_key_as_base64(), StrEq(expected_pub_key_data));

}

TEST_F(SSHKeyProvider, public_key_is_stable)
{
    mp::OpenSSHKeyProvider key_provider{key_dir.path(), QDir(key_dir.path()).filePath("fallback")};

    const auto key_one = key_provider.public_key_as_base64();
    const auto key_two = key_provider.public_key_as_base64();

    EXPECT_THAT(key_one, StrEq(key_two));
}
