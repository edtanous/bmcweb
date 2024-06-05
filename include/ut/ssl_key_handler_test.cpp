/*
 * SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "certs.hpp"

#include <unistd.h>

#include <ssl_key_handler.hpp>

#include <cstdlib>
#include <cstring>
#include <string>

#include <gtest/gtest.h>

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(SslKeyHandler, GivenFilename_WhenGenerateSslCertificate_FileIsCreated)
{
    ensuressl::generateSslCertificate(stubCertname, stubCn, &stubPkeyPwd);

    EXPECT_EQ(access(stubCertname.c_str(), F_OK), 0);

    remove(stubCertname.c_str());
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(
    SslKeyHandler,
    GivenGeneratedSslCertificateWithPassphrase_WhenCredentialFileIsReadWithNoPassphrase_PrivateKeyIsNotAccessible)
{
    ensuressl::generateSslCertificate(stubCertname, stubCn, &stubPkeyPwd);

    auto fp = fopen(stubCertname.c_str(), "r");
    auto pkey = PEM_read_PrivateKey(fp, nullptr, mockNoPwdCb, nullptr);
    fclose(fp);

    EXPECT_EQ(pkey, nullptr);

    EVP_PKEY_free(pkey);
    remove(stubCertname.c_str());
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(
    SslKeyHandler,
    GivenGeneratedSslCertificateWithPassphrase_WhenCredentialFileIsReadWithSamePassphrase_PrivateKeyIsAccessible)
{
    ensuressl::generateSslCertificate(stubCertname, stubCn, &stubPkeyPwd);

    auto fp = fopen(stubCertname.c_str(), "r");
    auto pkey = PEM_read_PrivateKey(fp, nullptr, mockPkeyPwdCb, nullptr);
    fclose(fp);

    EXPECT_NE(pkey, nullptr);

    EVP_PKEY_free(pkey);
    remove(stubCertname.c_str());
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(
    SslKeyHandler,
    GivenGeneratedSslCertificateFileWithPassphrase_WhenVerifyOpensslKeyCert_TrueIsReturned)
{
    ensuressl::generateSslCertificate(stubCertname, stubCn, &stubPkeyPwd);

    EXPECT_EQ(ensuressl::verifyOpensslKeyCert(stubCertname, mockPkeyPwdCb),
              true);

    remove(stubCertname.c_str());
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(
    SslKeyHandler,
    GivenGeneratedSslCertificateFileWithPassphrase_WhenVerifyOpensslKeyCertWithDifferentPassphrase_FalseIsReturned)
{
    ensuressl::generateSslCertificate(stubCertname, stubCn, &stubPkeyPwd);

    EXPECT_EQ(
        ensuressl::verifyOpensslKeyCert(stubCertname, mockPkeyInvalidPwdCb),
        false);

    remove(stubCertname.c_str());
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(
    SslKeyHandler,
    GivenFileWithNonencryptedPrivateKeyAndX509CertAndPassphraseAndEncryptCredentials_WhenPrivateKeyIsReadWithSamePassphrase_ItIsAccessible)
{
    auto fp = fopen(stubCertname.c_str(), "w");
    fputs(mockNonEncryptedCredFile.data(), fp);
    fclose(fp);

    ensuressl::encryptCredentials(stubCertname, &stubPkeyPwd);

    fp = fopen(stubCertname.c_str(), "r");
    auto pkey = PEM_read_PrivateKey(fp, nullptr, mockPkeyPwdCb, nullptr);
    fclose(fp);

    EXPECT_NE(pkey, nullptr);

    EVP_PKEY_free(pkey);
    remove(stubCertname.c_str());
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(
    SslKeyHandler,
    GivenFileWithUnencryptedPrivateKeyWithX509CertAndEncryptCredentialsWithPassphrase_WhenX509CertificateIsRead_CertificateIsUnchanged)
{
    auto fp = fopen(stubCertname.c_str(), "w");
    fputs(mockNonEncryptedCredFile.data(), fp);
    fclose(fp);

    ensuressl::encryptCredentials(stubCertname, &stubPkeyPwd);

    // Read cert from modified temporary file & original file bytes
    fp = fopen(stubCertname.c_str(), "r");
    auto x509 = PEM_read_X509(fp, nullptr, nullptr, nullptr);
    fclose(fp);
    auto bio = BIO_new(BIO_s_mem());
    BIO_write(bio, static_cast<const void*>(mockNonEncryptedCredFile.data()),
              static_cast<int>(mockNonEncryptedCredFileSize));
    auto x509Orig = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);

    EXPECT_EQ(X509_cmp(x509, x509Orig), 0);

    X509_free(x509);
    X509_free(x509Orig);
    BIO_free(bio);
    remove(stubCertname.c_str());
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(
    SslKeyHandler,
    GivenFileWithEncryptedPrivateKeyWithX509CertAndEncryptCredentialsWithSamePassphrase_WhenFileIsRead_NoChangesAreMade)
{
    std::array<char, static_cast<unsigned long>(BUFSIZ)> buf = {0};

    auto fp = fopen(stubCertname.c_str(), "w");
    fputs(mockEncryptedCredFile.data(), fp);
    fclose(fp);

    ensuressl::encryptCredentials(stubCertname, &stubPkeyPwd);

    fp = fopen(stubCertname.c_str(), "r");
    (void)!fread(static_cast<void*>(buf.data()), sizeof(char), BUFSIZ, fp);
    fclose(fp);

    EXPECT_EQ(memcmp(static_cast<void*>(buf.data()),
                     mockEncryptedCredFile.data(), mockEncryptedCredFileSize),
              0);

    remove(stubCertname.c_str());
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(
    SslKeyHandler,
    GivenFileWithEncryptedPrivateKeyWithX509CertAndEncryptCredentialsWithNoPassphrase_WhenFileIsRead_NoChangesAreMade)
{
    std::array<char, BUFSIZ> buf = {0};

    auto fp = fopen(stubCertname.c_str(), "w");
    fputs(mockEncryptedCredFile.data(), fp);
    fclose(fp);

    ensuressl::encryptCredentials(stubCertname, nullptr);

    fp = fopen(stubCertname.c_str(), "r");
    (void)!fread(static_cast<void*>(buf.data()), sizeof(char), BUFSIZ, fp);
    fclose(fp);

    EXPECT_EQ(memcmp(static_cast<void*>(buf.data()),
                     mockEncryptedCredFile.data(), mockEncryptedCredFileSize),
              0);

    remove(stubCertname.c_str());
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(
    SslKeyHandler,
    GivenNonexistentFilename_WhenEnsureOpensslKeyPresentEncryptedAndValid_FileIsCreated)
{
    ensuressl::ensureOpensslKeyPresentEncryptedAndValid(
        stubCertname, &stubPkeyPwd, mockPkeyPwdCb);

    EXPECT_EQ(access(stubCertname.c_str(), F_OK), 0);

    remove(stubCertname.c_str());
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(
    SslKeyHandler,
    GivenNonexistentFilenameAndPassphraseAndCallbackAndEnsureOpensslKeyPresentEncryptedAndValid_WhenPrivateKeyIsReadWithSamePassphrase_ItIsAccessible)
{
    ensuressl::ensureOpensslKeyPresentEncryptedAndValid(
        stubCertname, &stubPkeyPwd, mockPkeyPwdCb);

    auto fp = fopen(stubCertname.c_str(), "r");
    auto pkey = PEM_read_PrivateKey(fp, nullptr, mockPkeyPwdCb, nullptr);
    fclose(fp);

    EVP_PKEY_free(pkey);
    remove(stubCertname.c_str());
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(
    SslKeyHandler,
    GivenNonexistentFilenameAndPassphraseAndCallbackAndEnsureOpensslKeyPresentEncryptedAndValid_WhenX509CertificateIsRead_ItIsAccessible)
{
    ensuressl::ensureOpensslKeyPresentEncryptedAndValid(
        stubCertname, &stubPkeyPwd, mockPkeyPwdCb);

    auto fp = fopen(stubCertname.c_str(), "r");
    auto x509 = PEM_read_X509(fp, nullptr, nullptr, nullptr);
    fclose(fp);

    EXPECT_NE(x509, nullptr);

    X509_free(x509);
    remove(stubCertname.c_str());
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(
    SslKeyHandler,
    GivenUnencryptedPkeyAndX509CertFileAndPassphraseAndCallbackWhenEnsureOpensslKeyPresentEncryptedAndValid_WhenPrivateKeyIsReadWithSamePassphrase_ItIsAccessible)
{
    auto fp = fopen(stubCertname.c_str(), "w");
    fputs(mockNonEncryptedCredFile.data(), fp);
    fclose(fp);

    ensuressl::ensureOpensslKeyPresentEncryptedAndValid(
        stubCertname, &stubPkeyPwd, mockPkeyPwdCb);

    fp = fopen(stubCertname.c_str(), "r");
    auto pkey = PEM_read_PrivateKey(fp, nullptr, mockPkeyPwdCb, nullptr);
    fclose(fp);

    EXPECT_NE(pkey, nullptr);

    EVP_PKEY_free(pkey);
    remove(stubCertname.c_str());
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(
    SslKeyHandler,
    GivenUnencryptedPkeyAndX509CertFileAndPassphraseAndCallbackWhenEnsureOpensslKeyPresentEncryptedAndValid_WhenPrivateKeyIsReadWithDifferentPassphrase_ItIsNotAccessible)
{
    auto fp = fopen(stubCertname.c_str(), "w");
    fputs(mockNonEncryptedCredFile.data(), fp);
    fclose(fp);

    ensuressl::ensureOpensslKeyPresentEncryptedAndValid(
        stubCertname, &stubPkeyPwd, mockPkeyPwdCb);

    fp = fopen(stubCertname.c_str(), "r");
    auto pkey = PEM_read_PrivateKey(fp, nullptr, mockPkeyInvalidPwdCb, nullptr);
    fclose(fp);

    EXPECT_EQ(pkey, nullptr);

    EVP_PKEY_free(pkey);
    remove(stubCertname.c_str());
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(
    SslKeyHandler,
    GivenUnencryptedPkeyAndX509CertFileAndPassphraseAndCallbackWhenEnsureOpensslKeyPresentEncryptedAndValid_WhenX509CertificateIsRead_ItIsUnchanged)
{
    auto fp = fopen(stubCertname.c_str(), "w");
    fputs(mockNonEncryptedCredFile.data(), fp);
    fclose(fp);

    ensuressl::ensureOpensslKeyPresentEncryptedAndValid(
        stubCertname, &stubPkeyPwd, mockPkeyPwdCb);

    fp = fopen(stubCertname.c_str(), "r");
    auto x509 = PEM_read_X509(fp, nullptr, nullptr, nullptr);
    fclose(fp);
    auto bio = BIO_new(BIO_s_mem());
    BIO_write(bio, static_cast<const void*>(mockNonEncryptedCredFile.data()),
              static_cast<int>(mockNonEncryptedCredFileSize));
    auto x509Orig = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);

    EXPECT_EQ(X509_cmp(x509, x509Orig), 0);

    X509_free(x509);
    X509_free(x509Orig);
    BIO_free(bio);
    remove(stubCertname.c_str());
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(
    SslKeyHandler,
    GivenEncryptedPkeyAndX509CertFileUsingPassphraseAndEnsureOpensslKeyPresentEncryptedAndValidWithSamePassphraseCallback_WhenPrivateKeyIsReadWithSamePassphrase_ItIsAccessible)
{
    auto fp = fopen(stubCertname.c_str(), "w");
    fputs(mockEncryptedCredFile.data(), fp);
    fclose(fp);

    ensuressl::ensureOpensslKeyPresentEncryptedAndValid(
        stubCertname, &stubPkeyPwd, mockPkeyPwdCb);

    fp = fopen(stubCertname.c_str(), "r");
    auto pkey = PEM_read_PrivateKey(fp, nullptr, mockPkeyPwdCb, nullptr);
    fclose(fp);

    EXPECT_NE(pkey, nullptr);

    EVP_PKEY_free(pkey);
    remove(stubCertname.c_str());
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(
    SslKeyHandler,
    GivenEncryptedPkeyAndX509CertFileUsingPassphraseAndCallbackEnsureOpensslKeyPresentEncryptedAndValidWithSecondPassphrase_WhenPrivateKeyIsReadWithSecondPassphrase_ItIsRecreatedAndAccessible)
{
    auto fp = fopen(stubCertname.c_str(), "w");
    fputs(mockEncryptedCredFile.data(), fp);
    fclose(fp);

    ensuressl::ensureOpensslKeyPresentEncryptedAndValid(
        stubCertname, &stubPkeyInvalidPwd, mockPkeyInvalidPwdCb);

    fp = fopen(stubCertname.c_str(), "r");
    auto pkey = PEM_read_PrivateKey(fp, nullptr, mockPkeyInvalidPwdCb, nullptr);
    fclose(fp);

    EXPECT_NE(pkey, nullptr);

    EVP_PKEY_free(pkey);
    remove(stubCertname.c_str());
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(
    SslKeyHandler,
    GivenEncryptedPkeyAndX509CertFileUsingPassphraseAndEnsureOpensslKeyPresentEncryptedAndValidWithSecondPassphraseCallback_WhenPrivateKeyIsReadWithFirstPassphrase_ItIsNotAccessible)
{
    auto fp = fopen(stubCertname.c_str(), "w");
    fputs(mockEncryptedCredFile.data(), fp);
    fclose(fp);

    ensuressl::ensureOpensslKeyPresentEncryptedAndValid(
        stubCertname, &stubPkeyInvalidPwd, mockPkeyInvalidPwdCb);

    fp = fopen(stubCertname.c_str(), "r");
    auto pkey = PEM_read_PrivateKey(fp, nullptr, mockPkeyPwdCb, nullptr);
    fclose(fp);

    EXPECT_EQ(pkey, nullptr);

    EVP_PKEY_free(pkey);
    remove(stubCertname.c_str());
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(
    SslKeyHandler,
    GivenEncryptedPkeyAndX509CertFileUsingPassphraseAndEnsureOpensslKeyPresentEncryptedAndValidWithSecondPassphraseCallback_WhenX509CertificateIsRead_ItIsUnchanged)
{
    auto fp = fopen(stubCertname.c_str(), "w+");
    fputs(mockEncryptedCredFile.data(), fp);
    fseek(fp, 0, SEEK_SET);
    auto x509Orig = PEM_read_X509(fp, nullptr, nullptr, nullptr);
    fclose(fp);

    ensuressl::ensureOpensslKeyPresentEncryptedAndValid(
        stubCertname, &stubPkeyInvalidPwd, mockPkeyInvalidPwdCb);

    fp = fopen(stubCertname.c_str(), "r");
    auto x509 = PEM_read_X509(fp, nullptr, nullptr, nullptr);
    fclose(fp);

    EXPECT_NE(X509_cmp(x509, x509Orig), 0);

    X509_free(x509);
    X509_free(x509Orig);
    remove(stubCertname.c_str());
}
