// Host-side unit tests for the driver / HAL.
//
// Two kinds of tests live here:
//   * Guard/not-initialised tests use instances that are never initialised, so
//     the code returns at a NULL check before touching any register.
//   * Init/config tests (HalInit fixture) DO run the real init sequence. That
//     works because test_hw.h (force-included by CMake) redirects the driver's
//     base addresses at the fake_uart_mem[] RAM buffer instead of 0x09000000,
//     so register writes land in memory we can inspect - no segfault.
//
// Note: the driver's instance table is static/process-wide. ctest runs each
// test in its own process (so state is fresh), and the init tests use a
// dedicated instance (UART1) that the guard tests never touch, so the two
// groups don't interfere even if run together in one process.

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "application.h"   // pulls in HAL.h -> DRIVER.h -> PL011_config.h
}
#include "test_hw.h"       // fake_uart_mem[] - the RAM standing in for registers

// --- argument-validation guards (pure: they check args and return) ---
TEST(Guards, Error_check){
    UART_status status = HAL_UART_clear_errors(UART0);
    EXPECT_EQ(status, HAL_UART_ERR_NOT_INITIALIZED);
}

TEST(Guards, BaudRateZeroIsRejected) {
    EXPECT_EQ(HAL_UART_set_baud_rate(UART0, 0), HAL_UART_ERR_INVALID_ARG);
}

TEST(Guards, DataBitsOutOfRangeRejected) {
    EXPECT_EQ(HAL_UART_set_data_bits(UART0, 4), HAL_UART_ERR_INVALID_ARG);
    EXPECT_EQ(HAL_UART_set_data_bits(UART0, 9), HAL_UART_ERR_INVALID_ARG);
}

TEST(Guards, ReadTimeoutNullPointerRejected) {
    EXPECT_EQ(HAL_UART_read_timeout(UART0, nullptr, 10), HAL_UART_ERR_INVALID_ARG);
}

TEST(Guards, AppReceiveCharNullPointerRejected) {
    EXPECT_EQ(App_UART_ReceiveChar(UART0, nullptr), HAL_UART_ERR_INVALID_ARG);
}

TEST(NotInitialised, HalWriteOnUnopenedInstance) {
    EXPECT_EQ(HAL_UART_write(UART0, 'A'), HAL_UART_ERR_NOT_INITIALIZED);
}

TEST(NotInitialised, DriverWriteReturnsMinusOne) {
    EXPECT_EQ(drv_uart_write_UARTDR(UART0, 0x41), -1);
}

TEST(NotInitialised, OutOfRangeInstanceRejected) {
    EXPECT_EQ(drv_uart_write_UARTDR(UART_INSTANCE_COUNT, 0x41), -1);
}

// --- init / configuration tests -------------------------------------------
// These run the REAL HAL_UART_init against the fake register bank (test_hw.h),
// then read the bank back to confirm the right values were programmed. UART1 is
// used exclusively here so these never collide with the guard tests above.

class HalInit : public ::testing::Test {
protected:
    void SetUp() override {
        // Fresh, zeroed "registers" before each test so writes are unambiguous.
        std::memset(fake_uart_mem[UART1], 0, sizeof(fake_uart_mem[UART1]));
    }
    // View the fake bank as the driver's register layout.
    REGISTERS *regs() { return reinterpret_cast<REGISTERS *>(UART1_BASE); }
};

TEST_F(HalInit, ProgramsExpectedConfig) {
    ASSERT_EQ(HAL_UART_init(UART1, 9600), HAL_UART_STATUS_OK);

    REGISTERS *r = regs();
    EXPECT_EQ(r->UARTLCR_H.raw, 0x72u);   // PEN | FEN | WLEN=3
    EXPECT_EQ(r->UARTIBRD,      156u);    // 24 MHz / (16 * 9600), integer part
    EXPECT_EQ(r->UARTFBRD,      16u);     // fractional part
    EXPECT_EQ(r->UARTCR.raw,    0x301u);  // UARTEN | TXE | RXE
}

TEST_F(HalInit, BaudRateProgramsDivisors) {
    // A different rate exercises the divisor math end-to-end through the HAL.
    ASSERT_EQ(HAL_UART_init(UART1, 115200), HAL_UART_STATUS_OK);

    REGISTERS *r = regs();
    EXPECT_EQ(r->UARTIBRD, 13u);          // 24 MHz / (16 * 115200) -> 13
    EXPECT_EQ(r->UARTFBRD, 1u);
}
