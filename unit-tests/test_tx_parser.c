#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <cmocka.h>

#include "testing.h"
#include "../src/globals.h"
#include "../src/transaction.h"

static const char hexdigits[] = {
  '0', '1', '2', '3', '4', '5', '6', '7',
  '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

static void to_hex(unsigned char *input, int len, char *output){
  int i, outlen = 0;
  for(i=0; i<len; i++){
    unsigned char c = input[i];
    output[outlen++] = hexdigits[(c >> 4) & 0xf];
    output[outlen++] = hexdigits[c & 0xf];
  }
  output[outlen++] = 0;
}

static int parse_transaction(const unsigned char *buf, unsigned int len){
  bool is_first, is_last;
  unsigned char *ptr = (unsigned char *) buf;
  unsigned int remaining = len;
  int ret;

  ret = setjmp(jump_buffer);

  while (remaining > 0 && ret == 0) {
    unsigned int bytes_now = remaining;
    if (bytes_now > MAX_TX_PART) bytes_now = MAX_TX_PART;
    remaining -= bytes_now;
    is_first = (ptr == buf);
    is_last = (remaining == 0);
    parse_transaction_part(ptr, bytes_now, is_first, is_last);
    ptr += bytes_now;
  }

  return ret;
}

// TEST CASES -------------------------------

// NORMAL / LEGACY
static void test_tx_parsing_normal(void **state) {
    (void) state;
    char account_address[EncodedAddressLength+1];
    char *payload = NULL;
    char chainIdHex[65];

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x00,
        // transaction
        0x08, 0x01, 0x12, 0x21, 0x03, 0x4f, 0xea, 0xa6,
        0xed, 0xd6, 0xcf, 0x2a, 0x0e, 0x35, 0x5c, 0x88,
        0xe9, 0xbe, 0x9a, 0xc6, 0x98, 0x30, 0x83, 0x88,
        0x27, 0xbe, 0xda, 0xa3, 0x85, 0xc5, 0x81, 0x8e,
        0xb7, 0x25, 0xcb, 0x1d, 0x87, 0x1a, 0x21, 0x02,
        0x5d, 0x22, 0x30, 0xba, 0x75, 0x21, 0x7e, 0x60,
        0x37, 0x99, 0xe9, 0xa3, 0xd5, 0xb9, 0x1a, 0x63,
        0x61, 0x48, 0x3f, 0x9d, 0xa7, 0x37, 0x96, 0x41,
        0x0f, 0x6b, 0xc1, 0xce, 0x58, 0x01, 0xfd, 0xf2,
        0x22, 0x09, 0x06, 0xb1, 0x4b, 0xd1, 0xe6, 0xee,
        0xa0, 0x00, 0x00, 0x2a, 0x20, 0x30, 0x31, 0x30,
        0x32, 0x30, 0x33, 0x30, 0x34, 0x30, 0x35, 0x30,
        0x36, 0x30, 0x37, 0x30, 0x38, 0x30, 0x39, 0x30,
        0x41, 0x30, 0x42, 0x30, 0x43, 0x30, 0x44, 0x30,
        0x45, 0x30, 0x46, 0x46, 0x46, 0x3a, 0x01, 0x00,
        0x4a, 0x20, 0x52, 0x48, 0x45, 0xc2, 0x4c, 0xd3,
        0xe5, 0x3a, 0xec, 0xbc, 0xda, 0x8e, 0x31, 0x5d,
        0x62, 0xdc, 0x95, 0xa7, 0xf2, 0xf8, 0x25, 0x48,
        0x93, 0x0b, 0xc2, 0xfc, 0xc9, 0x86, 0xbf, 0x74,
        0x53, 0xbd,
    };

    memset(&txn, 0, sizeof(struct txn));

    int status = parse_transaction(raw_tx, sizeof(raw_tx));

    assert_int_equal(status, 0);

    encode_account(txn.account, 33, account_address, sizeof account_address);
    if (txn.payload_len > 0) {
      payload = malloc(txn.payload_len + 1);
      strncpy(payload, txn.payload, txn.payload_len);
    }
    to_hex(txn.chainId, 32, chainIdHex);

    assert_int_equal(txn.type, 0);
    assert_int_equal(txn.nonce, 1);
    assert_string_equal(account_address, "AmP4AYWHKrxnPqvoUATyJhMwarzJAphWdkosz24AWgiD2sQ18si9");
    assert_string_equal(recipient_address, "AmMDEyc36FNXB3Fq1a61HeVJRT4yssMEP11NWWE9Qx8yhfRKexvq");
    assert_string_equal(amount_str, "123.456 AERGO");
    assert_int_equal(txn.payload_len, 32);
    assert_string_equal(payload, "0102030405060708090A0B0C0D0E0FFF");
    assert_string_equal(txn.gasPrice, "");
    assert_string_equal(chainIdHex, "524845C24CD3E53AECBCDA8E315D62DC95A7F2F82548930BC2FCC986BF7453BD");
    assert_false(txn.is_name);
    assert_false(txn.is_system);
    assert_false(txn.is_enterprise);
}

// TRANSFER
static void test_tx_parsing_transfer(void **state) {
    (void) state;
    char account_address[EncodedAddressLength+1];
    char *payload = NULL;
    char chainIdHex[65];

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x04,
        // transaction
        0x08, 0x0a, 0x12, 0x21, 0x02, 0x9d, 0x02, 0x05,
        0x91, 0xe7, 0xfb, 0x7b, 0x09, 0x21, 0x53, 0x68,
        0x19, 0x95, 0xf8, 0x06, 0x09, 0xf0, 0xac, 0x98,
        0x8a, 0x4d, 0x93, 0x5e, 0x0e, 0xa6, 0x3c, 0x06,
        0x0f, 0x19, 0x54, 0xb0, 0x5f, 0x1a, 0x21, 0x03,
        0x8c, 0xb9, 0x2c, 0xde, 0xbf, 0x39, 0x98, 0x69,
        0x09, 0x3c, 0xac, 0x47, 0xe3, 0x70, 0xd8, 0xa9,
        0xfa, 0x50, 0x17, 0x30, 0x42, 0x23, 0xf9, 0xad,
        0x1a, 0x8c, 0x0a, 0x05, 0xa9, 0x06, 0xa9, 0xcb,
        0x22, 0x08, 0x14, 0xd1, 0x12, 0x0d, 0x7b, 0x16,
        0x00, 0x00, 0x3a, 0x01, 0x00, 0x40, 0x04, 0x4a,
        0x20, 0x52, 0x48, 0x45, 0xc2, 0x4c, 0xd3, 0xe5,
        0x3a, 0xec, 0xbc, 0xda, 0x8e, 0x31, 0x5d, 0x62,
        0xdc, 0x95, 0xa7, 0xf2, 0xf8, 0x25, 0x48, 0x93,
        0x0b, 0xc2, 0xfc, 0xc9, 0x86, 0xbf, 0x74, 0x53,
        0xbd,
    };

    memset(&txn, 0, sizeof(struct txn));

    int status = parse_transaction(raw_tx, sizeof(raw_tx));

    assert_int_equal(status, 0);

    encode_account(txn.account, 33, account_address, sizeof account_address);
    if (txn.payload_len > 0) {
      payload = malloc(txn.payload_len + 1);
      strncpy(payload, txn.payload, txn.payload_len);
    }
    to_hex(txn.chainId, 32, chainIdHex);

    assert_int_equal(txn.type, 4);
    assert_int_equal(txn.nonce, 10);
    assert_string_equal(account_address, "AmMhNZVhirdVrgL11koUh1j6TPnH118KqxdihFD9YXHD63VpyFGu");
    assert_string_equal(recipient_address, "AmPWwmdgpvPRPtykgCCWvVdZS6h7b6w9UzcLcsEd64mzKJ9RCAhp");
    assert_string_equal(amount_str, "1.5 AERGO");
    assert_int_equal(txn.payload_len, 0);
    assert_null(payload);
    assert_string_equal(txn.gasPrice, "");
    assert_string_equal(chainIdHex, "524845C24CD3E53AECBCDA8E315D62DC95A7F2F82548930BC2FCC986BF7453BD");
    assert_false(txn.is_name);
    assert_false(txn.is_system);
    assert_false(txn.is_enterprise);
}

// CALL
static void test_tx_parsing_call(void **state) {
    (void) state;
    char account_address[EncodedAddressLength+1];
    char *payload = NULL;
    char chainIdHex[65];

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x05,
        // transaction
        0x08, 0x19, 0x12, 0x21, 0x02, 0x9d, 0x02, 0x05,
        0x91, 0xe7, 0xfb, 0x7b, 0x09, 0x21, 0x53, 0x68,
        0x19, 0x95, 0xf8, 0x06, 0x09, 0xf0, 0xac, 0x98,
        0x8a, 0x4d, 0x93, 0x5e, 0x0e, 0xa6, 0x3c, 0x06,
        0x0f, 0x19, 0x54, 0xb0, 0x5f, 0x1a, 0x21, 0x03,
        0x8c, 0xb9, 0x2c, 0xde, 0xbf, 0x39, 0x98, 0x69,
        0x09, 0x3c, 0xac, 0x47, 0xe3, 0x70, 0xd8, 0xa9,
        0xfa, 0x50, 0x17, 0x30, 0x42, 0x23, 0xf9, 0xad,
        0x1a, 0x8c, 0x0a, 0x05, 0xa9, 0x06, 0xa9, 0xcb,
        0x22, 0x01, 0x00, 0x2a, 0x21, 0x7b, 0x22, 0x4e,
        0x61, 0x6d, 0x65, 0x22, 0x3a, 0x22, 0x68, 0x65,
        0x6c, 0x6c, 0x6f, 0x22, 0x2c, 0x22, 0x41, 0x72,
        0x67, 0x73, 0x22, 0x3a, 0x5b, 0x22, 0x77, 0x6f,
        0x72, 0x6c, 0x64, 0x22, 0x5d, 0x7d, 0x3a, 0x01,
        0x00, 0x40, 0x05, 0x4a, 0x20, 0x52, 0x48, 0x45,
        0xc2, 0x4c, 0xd3, 0xe5, 0x3a, 0xec, 0xbc, 0xda,
        0x8e, 0x31, 0x5d, 0x62, 0xdc, 0x95, 0xa7, 0xf2,
        0xf8, 0x25, 0x48, 0x93, 0x0b, 0xc2, 0xfc, 0xc9,
        0x86, 0xbf, 0x74, 0x53, 0xbd,
    };

    memset(&txn, 0, sizeof(struct txn));

    int status = parse_transaction(raw_tx, sizeof(raw_tx));

    assert_int_equal(status, 0);

    encode_account(txn.account, 33, account_address, sizeof account_address);
    if (txn.payload_len > 0) {
      payload = malloc(txn.payload_len + 1);
      strncpy(payload, txn.payload, txn.payload_len);
    }
    to_hex(txn.chainId, 32, chainIdHex);

    assert_int_equal(txn.type, 5);
    assert_int_equal(txn.nonce, 25);
    assert_string_equal(account_address, "AmMhNZVhirdVrgL11koUh1j6TPnH118KqxdihFD9YXHD63VpyFGu");
    assert_string_equal(recipient_address, "AmPWwmdgpvPRPtykgCCWvVdZS6h7b6w9UzcLcsEd64mzKJ9RCAhp");
    assert_string_equal(amount_str, "0 AERGO");
    assert_int_equal(txn.payload_len, 0x21);
    assert_string_equal(payload, "{\"Name\":\"hello\",\"Args\":[\"world\"]}");
    assert_string_equal(txn.gasPrice, "");
    assert_string_equal(chainIdHex, "524845C24CD3E53AECBCDA8E315D62DC95A7F2F82548930BC2FCC986BF7453BD");
    assert_false(txn.is_name);
    assert_false(txn.is_system);
    assert_false(txn.is_enterprise);
}

// MULTICALL
static void test_tx_parsing_multicall(void **state) {
    (void) state;
    char account_address[EncodedAddressLength+1];
    char *payload = NULL;
    char chainIdHex[65];

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x07,
        // transaction
        0x08, 0xfa, 0x01, 0x12, 0x21, 0x03, 0x8c, 0xb9,
        0x2c, 0xde, 0xbf, 0x39, 0x98, 0x69, 0x09, 0x3c,
        0xac, 0x47, 0xe3, 0x70, 0xd8, 0xa9, 0xfa, 0x50,
        0x17, 0x30, 0x42, 0x23, 0xf9, 0xad, 0x1a, 0x8c,
        0x0a, 0x05, 0xa9, 0x06, 0xa9, 0xcb, 0x22, 0x01,
        0x00, 0x2a, 0x4e, 0x5b, 0x5b, 0x22, 0x6c, 0x65,
        0x74, 0x22, 0x2c, 0x22, 0x6f, 0x62, 0x6a, 0x22,
        0x2c, 0x7b, 0x22, 0x6f, 0x6e, 0x65, 0x22, 0x3a,
        0x31, 0x2c, 0x22, 0x74, 0x77, 0x6f, 0x22, 0x3a,
        0x32, 0x7d, 0x5d, 0x2c, 0x5b, 0x22, 0x73, 0x65,
        0x74, 0x22, 0x2c, 0x22, 0x25, 0x6f, 0x62, 0x6a,
        0x25, 0x22, 0x2c, 0x22, 0x74, 0x68, 0x72, 0x65,
        0x65, 0x22, 0x2c, 0x33, 0x5d, 0x2c, 0x5b, 0x22,
        0x72, 0x65, 0x74, 0x75, 0x72, 0x6e, 0x22, 0x2c,
        0x22, 0x25, 0x6f, 0x62, 0x6a, 0x25, 0x22, 0x5d,
        0x5d, 0x3a, 0x01, 0x00, 0x40, 0x07, 0x4a, 0x20,
        0x52, 0x48, 0x45, 0xc2, 0x4c, 0xd3, 0xe5, 0x3a,
        0xec, 0xbc, 0xda, 0x8e, 0x31, 0x5d, 0x62, 0xdc,
        0x95, 0xa7, 0xf2, 0xf8, 0x25, 0x48, 0x93, 0x0b,
        0xc2, 0xfc, 0xc9, 0x86, 0xbf, 0x74, 0x53, 0xbd,
    };

    memset(&txn, 0, sizeof(struct txn));

    int status = parse_transaction(raw_tx, sizeof(raw_tx));

    assert_int_equal(status, 0);

    encode_account(txn.account, 33, account_address, sizeof account_address);
    if (txn.payload_len > 0) {
      payload = malloc(txn.payload_len + 1);
      strncpy(payload, txn.payload, txn.payload_len);
    }
    to_hex(txn.chainId, 32, chainIdHex);

    assert_int_equal(txn.type, 7);
    assert_int_equal(txn.nonce, 250);
    assert_string_equal(account_address, "AmPWwmdgpvPRPtykgCCWvVdZS6h7b6w9UzcLcsEd64mzKJ9RCAhp");
    assert_string_equal(recipient_address, "");
    assert_string_equal(amount_str, "0 AERGO");
    assert_int_equal(txn.payload_len, 0x4e);
    assert_string_equal(payload, "[[\"let\",\"obj\",{\"one\":1,\"two\":2}],[\"set\",\"%obj%\",\"three\",3],[\"return\",\"%obj%\"]]");
    assert_string_equal(txn.gasPrice, "");
    assert_string_equal(chainIdHex, "524845C24CD3E53AECBCDA8E315D62DC95A7F2F82548930BC2FCC986BF7453BD");
    assert_false(txn.is_name);
    assert_false(txn.is_system);
    assert_false(txn.is_enterprise);
}

// DEPLOY
static void test_tx_parsing_deploy(void **state) {
    (void) state;
    char account_address[EncodedAddressLength+1];
    char *payload = NULL;
    char chainIdHex[65];

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x06,
        // transaction
        0x08, 0x81, 0x08, 0x12, 0x21, 0x03, 0x8c, 0xb9,
        0x2c, 0xde, 0xbf, 0x39, 0x98, 0x69, 0x09, 0x3c,
        0xac, 0x47, 0xe3, 0x70, 0xd8, 0xa9, 0xfa, 0x50,
        0x17, 0x30, 0x42, 0x23, 0xf9, 0xad, 0x1a, 0x8c,
        0x0a, 0x05, 0xa9, 0x06, 0xa9, 0xcb, 0x22, 0x01,
        0x00, 0x2a, 0x40, 0x30, 0x31, 0x30, 0x32, 0x30,
        0x33, 0x30, 0x34, 0x30, 0x35, 0x30, 0x36, 0x30,
        0x37, 0x30, 0x38, 0x30, 0x39, 0x30, 0x41, 0x30,
        0x42, 0x30, 0x43, 0x30, 0x44, 0x30, 0x45, 0x30,
        0x46, 0x46, 0x46, 0x30, 0x31, 0x30, 0x32, 0x30,
        0x33, 0x30, 0x34, 0x30, 0x35, 0x30, 0x36, 0x30,
        0x37, 0x30, 0x38, 0x30, 0x39, 0x30, 0x41, 0x30,
        0x42, 0x30, 0x43, 0x30, 0x44, 0x30, 0x45, 0x30,
        0x46, 0x46, 0x46, 0x3a, 0x01, 0x00, 0x40, 0x06,
        0x4a, 0x20, 0x52, 0x48, 0x45, 0xc2, 0x4c, 0xd3,
        0xe5, 0x3a, 0xec, 0xbc, 0xda, 0x8e, 0x31, 0x5d,
        0x62, 0xdc, 0x95, 0xa7, 0xf2, 0xf8, 0x25, 0x48,
        0x93, 0x0b, 0xc2, 0xfc, 0xc9, 0x86, 0xbf, 0x74,
        0x53, 0xbd,
    };

    memset(&txn, 0, sizeof(struct txn));

    int status = parse_transaction(raw_tx, sizeof(raw_tx));

    assert_int_equal(status, 0);

    encode_account(txn.account, 33, account_address, sizeof account_address);
    if (txn.payload_len > 0) {
      payload = malloc(txn.payload_len + 1);
      strncpy(payload, txn.payload, txn.payload_len);
    }
    to_hex(txn.chainId, 32, chainIdHex);

    assert_int_equal(txn.type, 6);
    assert_int_equal(txn.nonce, 1025);
    assert_string_equal(account_address, "AmPWwmdgpvPRPtykgCCWvVdZS6h7b6w9UzcLcsEd64mzKJ9RCAhp");
    assert_string_equal(recipient_address, "");
    assert_string_equal(amount_str, "0 AERGO");
    assert_int_equal(txn.payload_len, 0x40);
    assert_string_equal(payload, "0102030405060708090A0B0C0D0E0FFF0102030405060708090A0B0C0D0E0FFF");
    assert_string_equal(txn.gasPrice, "");
    assert_string_equal(chainIdHex, "524845C24CD3E53AECBCDA8E315D62DC95A7F2F82548930BC2FCC986BF7453BD");
    assert_false(txn.is_name);
    assert_false(txn.is_system);
    assert_false(txn.is_enterprise);
}

// GOVERNANCE
static void test_tx_parsing_governance(void **state) {
    (void) state;
    char account_address[EncodedAddressLength+1];
    char *payload = NULL;
    char chainIdHex[65];

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x01,
        // transaction
        0x08, 0x82, 0x10, 0x12, 0x21, 0x03, 0x8c, 0xb9,
        0x2c, 0xde, 0xbf, 0x39, 0x98, 0x69, 0x09, 0x3c,
        0xac, 0x47, 0xe3, 0x70, 0xd8, 0xa9, 0xfa, 0x50,
        0x17, 0x30, 0x42, 0x23, 0xf9, 0xad, 0x1a, 0x8c,
        0x0a, 0x05, 0xa9, 0x06, 0xa9, 0xcb, 0x1a, 0x10,
        0x61, 0x65, 0x72, 0x67, 0x6f, 0x2e, 0x65, 0x6e,
        0x74, 0x65, 0x72, 0x70, 0x72, 0x69, 0x73, 0x65,
        0x22, 0x01, 0x00, 0x2a, 0x56, 0x7b, 0x22, 0x4e,
        0x61, 0x6d, 0x65, 0x22, 0x3a, 0x22, 0x61, 0x70,
        0x70, 0x65, 0x6e, 0x64, 0x41, 0x64, 0x6d, 0x69,
        0x6e, 0x22, 0x2c, 0x22, 0x41, 0x72, 0x67, 0x73,
        0x22, 0x3a, 0x5b, 0x22, 0x41, 0x6d, 0x4d, 0x44,
        0x45, 0x79, 0x63, 0x33, 0x36, 0x46, 0x4e, 0x58,
        0x42, 0x33, 0x46, 0x71, 0x31, 0x61, 0x36, 0x31,
        0x48, 0x65, 0x56, 0x4a, 0x52, 0x54, 0x34, 0x79,
        0x73, 0x73, 0x4d, 0x45, 0x50, 0x31, 0x31, 0x4e,
        0x57, 0x57, 0x45, 0x39, 0x51, 0x78, 0x38, 0x79,
        0x68, 0x66, 0x52, 0x4b, 0x65, 0x78, 0x76, 0x71,
        0x22, 0x5d, 0x7d, 0x3a, 0x01, 0x00, 0x40, 0x01,
        0x4a, 0x20, 0x52, 0x48, 0x45, 0xc2, 0x4c, 0xd3,
        0xe5, 0x3a, 0xec, 0xbc, 0xda, 0x8e, 0x31, 0x5d,
        0x62, 0xdc, 0x95, 0xa7, 0xf2, 0xf8, 0x25, 0x48,
        0x93, 0x0b, 0xc2, 0xfc, 0xc9, 0x86, 0xbf, 0x74,
        0x53, 0xbd,
    };

    memset(&txn, 0, sizeof(struct txn));

    int status = parse_transaction(raw_tx, sizeof(raw_tx));

    assert_int_equal(status, 0);

    encode_account(txn.account, 33, account_address, sizeof account_address);
    if (txn.payload_len > 0) {
      payload = malloc(txn.payload_len + 1);
      strncpy(payload, txn.payload, txn.payload_len);
    }
    to_hex(txn.chainId, 32, chainIdHex);

    assert_int_equal(txn.type, 1);
    assert_int_equal(txn.nonce, 2050);
    assert_string_equal(account_address, "AmPWwmdgpvPRPtykgCCWvVdZS6h7b6w9UzcLcsEd64mzKJ9RCAhp");
    assert_string_equal(recipient_address, "aergo.enterprise");
    assert_string_equal(amount_str, "0 AERGO");
    assert_int_equal(txn.payload_len, 0x56);
    assert_string_equal(payload, "{\"Name\":\"appendAdmin\",\"Args\":[\"AmMDEyc36FNXB3Fq1a61HeVJRT4yssMEP11NWWE9Qx8yhfRKexvq\"]}");
    assert_string_equal(txn.gasPrice, "");
    assert_string_equal(chainIdHex, "524845C24CD3E53AECBCDA8E315D62DC95A7F2F82548930BC2FCC986BF7453BD");
    assert_false(txn.is_name);
    assert_false(txn.is_system);
    assert_true(txn.is_enterprise);
}

// INVALID CONTENT --------------

// DIFFERENT TYPE
static void test_tx_parsing_diff_type(void **state) {
    (void) state;

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x05,
        // transaction
        0x08, 0x01, 0x12, 0x21, 0x03, 0x4f, 0xea, 0xa6,
        0xed, 0xd6, 0xcf, 0x2a, 0x0e, 0x35, 0x5c, 0x88,
        0xe9, 0xbe, 0x9a, 0xc6, 0x98, 0x30, 0x83, 0x88,
        0x27, 0xbe, 0xda, 0xa3, 0x85, 0xc5, 0x81, 0x8e,
        0xb7, 0x25, 0xcb, 0x1d, 0x87, 0x1a, 0x21, 0x02,
        0x5d, 0x22, 0x30, 0xba, 0x75, 0x21, 0x7e, 0x60,
        0x37, 0x99, 0xe9, 0xa3, 0xd5, 0xb9, 0x1a, 0x63,
        0x61, 0x48, 0x3f, 0x9d, 0xa7, 0x37, 0x96, 0x41,
        0x0f, 0x6b, 0xc1, 0xce, 0x58, 0x01, 0xfd, 0xf2,
        0x22, 0x09, 0x06, 0xb1, 0x4b, 0xd1, 0xe6, 0xee,
        0xa0, 0x00, 0x00, 0x2a, 0x20, 0x30, 0x31, 0x30,
        0x32, 0x30, 0x33, 0x30, 0x34, 0x30, 0x35, 0x30,
        0x36, 0x30, 0x37, 0x30, 0x38, 0x30, 0x39, 0x30,
        0x41, 0x30, 0x42, 0x30, 0x43, 0x30, 0x44, 0x30,
        0x45, 0x30, 0x46, 0x46, 0x46, 0x3a, 0x01, 0x00,
        0x4a, 0x20, 0x52, 0x48, 0x45, 0xc2, 0x4c, 0xd3,
        0xe5, 0x3a, 0xec, 0xbc, 0xda, 0x8e, 0x31, 0x5d,
        0x62, 0xdc, 0x95, 0xa7, 0xf2, 0xf8, 0x25, 0x48,
        0x93, 0x0b, 0xc2, 0xfc, 0xc9, 0x86, 0xbf, 0x74,
        0x53, 0xbd,
    };

    memset(&txn, 0, sizeof(struct txn));

    int status = parse_transaction(raw_tx, sizeof(raw_tx));

    assert_int_equal(status, 0x6728);
}

// WITHOUT NONCE
static void test_tx_parsing_without_nonce(void **state) {
    (void) state;

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x00,
        // transaction
        0x12, 0x21, 0x03, 0x4f, 0xea, 0xa6,
        0xed, 0xd6, 0xcf, 0x2a, 0x0e, 0x35, 0x5c, 0x88,
        0xe9, 0xbe, 0x9a, 0xc6, 0x98, 0x30, 0x83, 0x88,
        0x27, 0xbe, 0xda, 0xa3, 0x85, 0xc5, 0x81, 0x8e,
        0xb7, 0x25, 0xcb, 0x1d, 0x87, 0x1a, 0x21, 0x02,
        0x5d, 0x22, 0x30, 0xba, 0x75, 0x21, 0x7e, 0x60,
        0x37, 0x99, 0xe9, 0xa3, 0xd5, 0xb9, 0x1a, 0x63,
        0x61, 0x48, 0x3f, 0x9d, 0xa7, 0x37, 0x96, 0x41,
        0x0f, 0x6b, 0xc1, 0xce, 0x58, 0x01, 0xfd, 0xf2,
        0x22, 0x09, 0x06, 0xb1, 0x4b, 0xd1, 0xe6, 0xee,
        0xa0, 0x00, 0x00, 0x2a, 0x20, 0x30, 0x31, 0x30,
        0x32, 0x30, 0x33, 0x30, 0x34, 0x30, 0x35, 0x30,
        0x36, 0x30, 0x37, 0x30, 0x38, 0x30, 0x39, 0x30,
        0x41, 0x30, 0x42, 0x30, 0x43, 0x30, 0x44, 0x30,
        0x45, 0x30, 0x46, 0x46, 0x46, 0x3a, 0x01, 0x00,
        0x4a, 0x20, 0x52, 0x48, 0x45, 0xc2, 0x4c, 0xd3,
        0xe5, 0x3a, 0xec, 0xbc, 0xda, 0x8e, 0x31, 0x5d,
        0x62, 0xdc, 0x95, 0xa7, 0xf2, 0xf8, 0x25, 0x48,
        0x93, 0x0b, 0xc2, 0xfc, 0xc9, 0x86, 0xbf, 0x74,
        0x53, 0xbd,
    };

    memset(&txn, 0, sizeof(struct txn));

    int status = parse_transaction(raw_tx, sizeof(raw_tx));

    assert_int_equal(status, 0x6721);
}

// WITHOUT ACCOUNT
static void test_tx_parsing_without_account(void **state) {
    (void) state;

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x00,
        // transaction
        0x08, 0x01,
        0x1a, 0x21, 0x02,
        0x5d, 0x22, 0x30, 0xba, 0x75, 0x21, 0x7e, 0x60,
        0x37, 0x99, 0xe9, 0xa3, 0xd5, 0xb9, 0x1a, 0x63,
        0x61, 0x48, 0x3f, 0x9d, 0xa7, 0x37, 0x96, 0x41,
        0x0f, 0x6b, 0xc1, 0xce, 0x58, 0x01, 0xfd, 0xf2,
        0x22, 0x09, 0x06, 0xb1, 0x4b, 0xd1, 0xe6, 0xee,
        0xa0, 0x00, 0x00, 0x2a, 0x20, 0x30, 0x31, 0x30,
        0x32, 0x30, 0x33, 0x30, 0x34, 0x30, 0x35, 0x30,
        0x36, 0x30, 0x37, 0x30, 0x38, 0x30, 0x39, 0x30,
        0x41, 0x30, 0x42, 0x30, 0x43, 0x30, 0x44, 0x30,
        0x45, 0x30, 0x46, 0x46, 0x46, 0x3a, 0x01, 0x00,
        0x4a, 0x20, 0x52, 0x48, 0x45, 0xc2, 0x4c, 0xd3,
        0xe5, 0x3a, 0xec, 0xbc, 0xda, 0x8e, 0x31, 0x5d,
        0x62, 0xdc, 0x95, 0xa7, 0xf2, 0xf8, 0x25, 0x48,
        0x93, 0x0b, 0xc2, 0xfc, 0xc9, 0x86, 0xbf, 0x74,
        0x53, 0xbd,
    };

    memset(&txn, 0, sizeof(struct txn));

    int status = parse_transaction(raw_tx, sizeof(raw_tx));

    assert_int_equal(status, 0x6722);
}

// WITHOUT CHAIN_ID
static void test_tx_parsing_without_chain_id(void **state) {
    (void) state;

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x00,
        // transaction
        0x08, 0x01, 0x12, 0x21, 0x03, 0x4f, 0xea, 0xa6,
        0xed, 0xd6, 0xcf, 0x2a, 0x0e, 0x35, 0x5c, 0x88,
        0xe9, 0xbe, 0x9a, 0xc6, 0x98, 0x30, 0x83, 0x88,
        0x27, 0xbe, 0xda, 0xa3, 0x85, 0xc5, 0x81, 0x8e,
        0xb7, 0x25, 0xcb, 0x1d, 0x87, 0x1a, 0x21, 0x02,
        0x5d, 0x22, 0x30, 0xba, 0x75, 0x21, 0x7e, 0x60,
        0x37, 0x99, 0xe9, 0xa3, 0xd5, 0xb9, 0x1a, 0x63,
        0x61, 0x48, 0x3f, 0x9d, 0xa7, 0x37, 0x96, 0x41,
        0x0f, 0x6b, 0xc1, 0xce, 0x58, 0x01, 0xfd, 0xf2,
        0x22, 0x09, 0x06, 0xb1, 0x4b, 0xd1, 0xe6, 0xee,
        0xa0, 0x00, 0x00, 0x2a, 0x20, 0x30, 0x31, 0x30,
        0x32, 0x30, 0x33, 0x30, 0x34, 0x30, 0x35, 0x30,
        0x36, 0x30, 0x37, 0x30, 0x38, 0x30, 0x39, 0x30,
        0x41, 0x30, 0x42, 0x30, 0x43, 0x30, 0x44, 0x30,
        0x45, 0x30, 0x46, 0x46, 0x46, 0x3a, 0x01, 0x00,
        0x4B, 0x20, 0x52, 0x48, 0x45, 0xc2, 0x4c, 0xd3,  // 4B instead of 4A
        0xe5, 0x3a, 0xec, 0xbc, 0xda, 0x8e, 0x31, 0x5d,
        0x62, 0xdc, 0x95, 0xa7, 0xf2, 0xf8, 0x25, 0x48,
        0x93, 0x0b, 0xc2, 0xfc, 0xc9, 0x86, 0xbf, 0x74,
        0x53, 0xbd,
    };

    memset(&txn, 0, sizeof(struct txn));

    int status = parse_transaction(raw_tx, sizeof(raw_tx));

    assert_int_equal(status, 0x6729);
}

// PARTIAL CHAIN_ID - INCOMPLETE TXN
static void test_tx_parsing_incomplete_txn(void **state) {
    (void) state;

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x00,
        // transaction
        0x08, 0x01, 0x12, 0x21, 0x03, 0x4f, 0xea, 0xa6,
        0xed, 0xd6, 0xcf, 0x2a, 0x0e, 0x35, 0x5c, 0x88,
        0xe9, 0xbe, 0x9a, 0xc6, 0x98, 0x30, 0x83, 0x88,
        0x27, 0xbe, 0xda, 0xa3, 0x85, 0xc5, 0x81, 0x8e,
        0xb7, 0x25, 0xcb, 0x1d, 0x87, 0x1a, 0x21, 0x02,
        0x5d, 0x22, 0x30, 0xba, 0x75, 0x21, 0x7e, 0x60,
        0x37, 0x99, 0xe9, 0xa3, 0xd5, 0xb9, 0x1a, 0x63,
        0x61, 0x48, 0x3f, 0x9d, 0xa7, 0x37, 0x96, 0x41,
        0x0f, 0x6b, 0xc1, 0xce, 0x58, 0x01, 0xfd, 0xf2,
        0x22, 0x09, 0x06, 0xb1, 0x4b, 0xd1, 0xe6, 0xee,
        0xa0, 0x00, 0x00, 0x2a, 0x20, 0x30, 0x31, 0x30,
        0x32, 0x30, 0x33, 0x30, 0x34, 0x30, 0x35, 0x30,
        0x36, 0x30, 0x37, 0x30, 0x38, 0x30, 0x39, 0x30,
        0x41, 0x30, 0x42, 0x30, 0x43, 0x30, 0x44, 0x30,
        0x45, 0x30, 0x46, 0x46, 0x46, 0x3a, 0x01, 0x00,
        0x4a, 0x20, 0x52, 0x48, 0x45, 0xc2, 0x4c, 0xd3,
        0xe5, 0x3a, 0xec, 0xbc, 0xda, 0x8e, 0x31, 0x5d,
        0x62, 0xdc, 0x95, 0xa7, 0xf2, 0xf8, 0x25, 0x48,
        0x93, 0x0b, 0xc2, 0xfc, 0xc9, 0x86, 0xbf, 0x74,
    };

    memset(&txn, 0, sizeof(struct txn));

    int status = parse_transaction(raw_tx, sizeof(raw_tx));

    assert_int_equal(status, 0x6740);
}


int main() {
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_tx_parsing_normal),
      cmocka_unit_test(test_tx_parsing_transfer),
      cmocka_unit_test(test_tx_parsing_call),
      cmocka_unit_test(test_tx_parsing_multicall),
      cmocka_unit_test(test_tx_parsing_deploy),
      cmocka_unit_test(test_tx_parsing_governance),
      // invalid content
      cmocka_unit_test(test_tx_parsing_diff_type),
      cmocka_unit_test(test_tx_parsing_without_nonce),
      cmocka_unit_test(test_tx_parsing_without_account),
      cmocka_unit_test(test_tx_parsing_without_chain_id),
      cmocka_unit_test(test_tx_parsing_incomplete_txn),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
