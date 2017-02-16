#include <config.h>

#include "waveshare_ADS1256.h"
#include "bits.h"

#include <bcm2835.h>

#include <vector>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <functional>
#include <atomic>

#ifndef WORDS_BIGENDIAN
#error missing endian information
#endif


//CS    -----   SPICS_ADC
//DIN   -----   MOSI
//DOUT  -----   MISO
//SCLK  -----   SCLK
//DRDY  -----   ctl_IO     data  starting
//RST   -----   ctl_IO     reset

#define DRDY  RPI_GPIO_P1_11
#define RST   RPI_GPIO_P1_12
#define PDWN  RPI_GPIO_P1_13
#define SPICS_ADC RPI_GPIO_P1_15


namespace b = boost;
namespace po = boost::program_options;


enum {
	/*Register address, followed by reset the default values */
	REG_STATUS = 0,	// x1H
	REG_MUX    = 1, // 01H
	REG_ADCON  = 2, // 20H
	REG_DRATE  = 3, // F0H
	REG_IO     = 4, // E0H
	REG_OFC0   = 5, // xxH
	REG_OFC1   = 6, // xxH
	REG_OFC2   = 7, // xxH
	REG_FSC0   = 8, // xxH
	REG_FSC1   = 9, // xxH
	REG_FSC2   = 10 // xxH
};

/* Command definition£∫ TTable 24. Command Definitions --- ADS1256 datasheet Page 34 */
enum {
	CMD_WAKEUP  = 0x00,	// Completes SYNC and Exits Standby Mode 0000  0000 (00h)
	CMD_RDATA   = 0x01, // Read Data 0000  0001 (01h)
	CMD_RDATAC  = 0x03, // Read Data Continuously 0000   0011 (03h)
	CMD_SDATAC  = 0x0F, // Stop Read Data Continuously 0000   1111 (0Fh)
	CMD_RREG    = 0x10, // Read from REG rrr 0001 rrrr (1xh)
	CMD_WREG    = 0x50, // Write to REG rrr 0101 rrrr (5xh)
	CMD_SELFCAL = 0xF0, // Offset and Gain Self-Calibration 1111    0000 (F0h)
	CMD_SELFOCAL= 0xF1, // Offset Self-Calibration 1111    0001 (F1h)
	CMD_SELFGCAL= 0xF2, // Gain Self-Calibration 1111    0010 (F2h)
	CMD_SYSOCAL = 0xF3, // System Offset Calibration 1111   0011 (F3h)
	CMD_SYSGCAL = 0xF4, // System Gain Calibration 1111    0100 (F4h)
	CMD_SYNC    = 0xFC, // Synchronize the A/D Conversion 1111   1100 (FCh)
	CMD_STANDBY = 0xFD, // Begin Standby Mode 1111   1101 (FDh)
	CMD_RESET   = 0xFE // Reset to Power-Up Values 1111   1110 (FEh)
};


namespace waveshare {











/**
    Waveshare expansion board does not use the normal SPI chip-select
    machinery. That is, it does not use the RPi's native chip select pins
    but rather uses other GPIO. I can only assume this is to allow the
    system to talk to other SPI devices as needed.

    In any case, the ADS1256 is active low chip select
 */
static inline void SPI_release_ADC(void)
{
  bcm2835_gpio_write(SPICS_ADC,HIGH);
}

static inline void SPI_assert_ADC(void)
{
  bcm2835_gpio_write(SPICS_ADC,LOW);
}

/**
    Spin wait on DRDY

    OK, this is stupid as a low sample rate will cause the CPU to block but
    for now this works. Need to rework low sample rate code to just manually
    poll and sleep instead of waiting on the DRDY to go low. An better
    alternative is to get GPIO interrupts working but that will likely need
    this to be run in the kernel which is a major rewrite
 */
static inline void wait_DRDY(void)
{
  // Depending on what we are doing, this may make more sense as an interrupt
  while(bcm2835_gpio_lev(DRDY) != 0) {
    // Wait forever.
  }
}

/*
  Write to num registers starting at \c reg_start. \c data is expected to be
  at least num bytes long
*/
void write_to_registers(uint8_t reg_start, char *data, uint8_t num)
{
  assert(reg_start <= 10 && num > 0);

  SPI_assert_ADC();

  // first nibble is the command, second is the register number
  bcm2835_spi_transfer(CMD_WREG | reg_start);
  // send the number of registers to write to - 1
  bcm2835_spi_transfer(num-1);

  bcm2835_spi_writenb(data,num);

  SPI_release_ADC();
}


























void waveshare_ADS1256::setup_com(void)
{
  // delay initialization of these so that we can check configuration options
  // first
  bcm2835lib_sentry.reset(new bcm2835_sentry());

  // MSBFIRST is the only supported BIT order according to the bcm2835 library
  // and appears to be the preferred order according to the ADS1255/6 datasheet
  bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);

  // not clear why mode 1 is being used. Need to clarify with datasheet
  bcm2835_spi_setDataMode(BCM2835_SPI_MODE1);

  // original had 1024 = 4.096us = 244.140625kHz from 250MHz system clock
  // not sure why this was chosen
  // Board clock is nominal 7.68 MHz or 130.2083 ns period
  // SCLK period is 4*1/fCLKIN per datasheet or 4*130.2083 = 520.8332 ns
  // This is optimal. SPI driver appears to only support clockdividers
  // that are a power of two. Thus, we should use
  // 256 = 1.024us = 976.5625MHz from 250MHz system clock
  // Might be possible to get away with a divider of 128, ie
  // 128 = 512ns = 1.953MHz from 250MHz system clock
  // Cycling throughput for 521 ns is 4374 with 30,000 SPS setting. Thus we
  // should expect to see lower performace. On my system, I achieve
  // ~0.24 ms sample period or about 4,166.6 interleaved samples per second
  bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_256);

  // set RPI_GPIO_P1_15 as output and set it high
  // Waveshare board does not use the normal SPI CS lines but rather uses
  // GPIO 15 for the ADC slave select requiring manual asserting
  bcm2835_gpio_fsel(RPI_GPIO_P1_15, BCM2835_GPIO_FSEL_OUTP);
  bcm2835_gpio_write(RPI_GPIO_P1_15, HIGH);

  // set RPI_GPIO_P1_11 ans INPUT and set as pull-up resistor
  bcm2835_gpio_fsel(RPI_GPIO_P1_11, BCM2835_GPIO_FSEL_INPT);
  bcm2835_gpio_set_pud(RPI_GPIO_P1_11, BCM2835_GPIO_PUD_UP);
}

void waveshare_ADS1256::initialize(void)
{
  // probably should force reset first

  char regs[4] = {0};

  //STATUS : STATUS REGISTER (ADDRESS 00h);
  //  MSB first, enable auto-cal, set input buffer
  regs[0] = (0 << 3) | (1 << 2) | (buffer_enabled << 1);

  //MUX : Input Multiplexer Control Register (Address 01h)
  regs[1] = 0x08; // for now

  //ADCON: A/D Control Register (Address 02h)
  //  Turn off clock out
  regs[2] = (0 << 5) | (0 << 2) | _gain_code;

  //DRATE: A/D Data Rate (Address 03h)
  //  Set the sample rate
  regs[3] = _sample_rate_code;

  write_to_registers(REG_STATUS,regs,4);

  // ADC should now start to auto-cal, DRDY goes low when done
  wait_DRDY();

}


void waveshare_ADS1256::trigger_sampling(const data_handler &handler,
  basic_trigger &trigger)
{
#if 0
  if(_async) {
    if(_stats)
      trigger_sampling_async_wstat_impl(handler,trigger);
    else
      trigger_sampling_async_impl(handler,trigger);
  }
  else {
    if(_stats)
      trigger_sampling_wstat_impl(handler,trigger);
    else
      trigger_sampling_impl(handler,trigger);
  }
#endif
}

void waveshare_ADS1256::finalize(void)
{
// this really needs to be setup as one and used only for this class. there is
// no real way to guarantee (or someone might forget) the order of the sentry
// destruction. ie the library could deallocate before the SPI. So just use
// one sentry class for both.
  bcm2835lib_sentry.reset();
}











#if 0

void waveshare_ADS1256::trigger_sampling_impl(const data_handler &handler,
  basic_trigger &trigger)
{
  sample_buffer_type sample_buffer(
    row_block*channel_assignment.size()*bit_depth());

  trigger.wait_start();

  // cycle through once and throw away data to set per-channel statistics and
  // ensure valid data on first "real" sample
  char dummy_buf[3];
  for(std::size_t chan=0;
    chan<channel_assignment.size() && !trigger.should_stop();
    ++chan)
  {
    // 2,083.3328 usec max wait
    wait_DRDY();

    // switch to the next channel
    write_to_registers(REG_MUX,
      &(channel_assignment[(chan+1)%channel_assignment.size()]),1);
    bcm2835_delayMicroseconds(5);

    SPI_assert_ADC();
    bcm2835_spi_transfer(CMD_SYNC);
    SPI_release_ADC();
    bcm2835_delayMicroseconds(5);

    SPI_assert_ADC();
    bcm2835_spi_transfer(CMD_WAKEUP);
    SPI_release_ADC();
    bcm2835_delayMicroseconds(25);

    SPI_assert_ADC();
    bcm2835_spi_transfer(CMD_RDATA);
    bcm2835_delayMicroseconds(10);

    bcm2835_spi_transfern(dummy_buf,3);
    SPI_release_ADC();
  }

  // correct channel is now staged for conversion
  bool done = false;
  while(!done && !trigger.should_stop()) {
    // cycling through the channels is done with a one cycle lag. That is,
    // while we are pulling the converted data off of the ADC's register, we
    // have already switched the conversion hardware to the next channel so that
    // off it can be settling down while we are in the process of pulling the
    // data for the previous conversion. See the datasheet pg. 21
    sample_buffer_type::pointer data_buffer = sample_buffer.data();

    std::size_t rows;
    for(rows=0; rows<row_block && !trigger.should_stop(); ++rows) {
      for(std::size_t chan=0; chan<channel_assignment.size(); ++chan) {
        // 2,083.3328 usec max wait
        wait_DRDY();

        // switch to the next channel
        write_to_registers(REG_MUX,
          &(channel_assignment[(chan+1)%channel_assignment.size()]),1);
        bcm2835_delayMicroseconds(5);

        SPI_assert_ADC();
        bcm2835_spi_transfer(CMD_SYNC);
        SPI_release_ADC();
        bcm2835_delayMicroseconds(5);

        SPI_assert_ADC();
        bcm2835_spi_transfer(CMD_WAKEUP);
        SPI_release_ADC();
        bcm2835_delayMicroseconds(25);

        SPI_assert_ADC();
        bcm2835_spi_transfer(CMD_RDATA);
        bcm2835_delayMicroseconds(10);

        bcm2835_spi_transfern(data_buffer,3);
        SPI_release_ADC();

        data_buffer += 3;
      }
    }

    done = handler(sample_buffer.data(),rows,*this);
  }
}

void waveshare_ADS1256::trigger_sampling_wstat_impl(const data_handler &handler,
  basic_trigger &trigger)
{
  typedef std::chrono::high_resolution_clock::time_point time_point_type;
  typedef std::chrono::nanoseconds::rep nanosecond_rep_type;

  static const std::size_t time_size = sizeof(nanosecond_rep_type);

  sample_buffer_type sample_buffer(
    row_block*channel_assignment.size()*(bit_depth()/8+time_size));

  trigger.wait_start();

  // cycle through once and throw away data to set per-channel statistics and
  // ensure valid data on first "real" sample
  char dummy_buf[3];
  for(std::size_t chan=0;
    chan<channel_assignment.size() && !trigger.should_stop();
    ++chan)
  {
    // 2,083.3328 usec max wait
    wait_DRDY();

    // switch to the next channel
    write_to_registers(REG_MUX,
      &(channel_assignment[(chan+1)%channel_assignment.size()]),1);
    bcm2835_delayMicroseconds(5);

    SPI_assert_ADC();
    bcm2835_spi_transfer(CMD_SYNC);
    SPI_release_ADC();
    bcm2835_delayMicroseconds(5);

    SPI_assert_ADC();
    bcm2835_spi_transfer(CMD_WAKEUP);
    SPI_release_ADC();
    bcm2835_delayMicroseconds(25);

    SPI_assert_ADC();
    bcm2835_spi_transfer(CMD_RDATA);
    bcm2835_delayMicroseconds(10);

    bcm2835_spi_transfern(dummy_buf,3);
    SPI_release_ADC();
  }

  time_point_type start_time = std::chrono::high_resolution_clock::now();

  // correct channel is now staged for conversion
  bool done = false;
  while(!done && !trigger.should_stop()) {
    // cycling through the channels is done with a one cycle lag. That is,
    // while we are pulling the converted data off of the ADC's register, we
    // have already switched the conversion hardware to the next channel so that
    // off it can be settling down while we are in the process of pulling the
    // data for the previous conversion. See the datasheet pg. 21
    sample_buffer_type::pointer data_buffer = sample_buffer.data();

    std::size_t rows;
    for(rows=0; rows<row_block && !trigger.should_stop(); ++rows) {
      for(std::size_t chan=0; chan<channel_assignment.size(); ++chan) {
        // 2,083.3328 usec max wait
        wait_DRDY();

        // switch to the next channel
        write_to_registers(REG_MUX,
          &(channel_assignment[(chan+1)%channel_assignment.size()]),1);
        bcm2835_delayMicroseconds(5);

        SPI_assert_ADC();
        bcm2835_spi_transfer(CMD_SYNC);
        SPI_release_ADC();
        bcm2835_delayMicroseconds(5);

        SPI_assert_ADC();
        bcm2835_spi_transfer(CMD_WAKEUP);
        SPI_release_ADC();
        bcm2835_delayMicroseconds(25);

        SPI_assert_ADC();
        bcm2835_spi_transfer(CMD_RDATA);
        bcm2835_delayMicroseconds(10);

        bcm2835_spi_transfern(data_buffer,3);
        SPI_release_ADC();

        std::chrono::high_resolution_clock::time_point now =
          std::chrono::high_resolution_clock::now();

        std::chrono::nanoseconds::rep elapsed =
          std::chrono::duration_cast<std::chrono::nanoseconds>(
            now-start_time).count();

        data_buffer += 3;
        elapsed = detail::ensure_be(elapsed);
        std::memcpy(data_buffer,&elapsed,time_size);
        data_buffer += time_size;
      }
    }

    done = handler(sample_buffer.data(),rows,*this);
  }
}

void waveshare_ADS1256::async_handler(ringbuffer_type &allocation_ringbuffer,
  ringbuffer_type &ready_ringbuffer, const data_handler &handler,
  std::atomic_int &done)
{
  sample_buffer_ptr sample_buffer;
  while(!done.load()) {
    if(!ready_ringbuffer.pop(sample_buffer))
      continue;

    done.fetch_or(handler(sample_buffer->data(),row_block,*this));

    allocation_ringbuffer.push(sample_buffer);
  }
}


void waveshare_ADS1256::trigger_sampling_async_impl(const data_handler &handler,
  basic_trigger &trigger)
{
  static const std::size_t max_allocation = 32;

  ringbuffer_type allocation_ringbuffer(max_allocation);
  ringbuffer_type ready_ringbuffer(max_allocation);

  std::size_t buffer_size =
    row_block*channel_assignment.size()*(bit_depth()/8);

  for(std::size_t i=0; i<max_allocation; ++i)
    allocation_ringbuffer.push(
      sample_buffer_ptr(new sample_buffer_type(buffer_size)));

  std::atomic_int done(false);

  std::thread servicing_thread(&waveshare_ADS1256::async_handler,*this,
    std::ref(allocation_ringbuffer), std::ref(ready_ringbuffer),
    std::cref(handler), std::ref(done));

  trigger.wait_start();

  // cycle through once and throw away data to set per-channel statistics and
  // ensure valid data on first "real" sample
  char dummy_buf[3];
  for(std::size_t chan=0;
    chan<channel_assignment.size() && !trigger.should_stop();
    ++chan)
  {
    // 2,083.3328 usec max wait
    wait_DRDY();

    // switch to the next channel
    write_to_registers(REG_MUX,
      &(channel_assignment[(chan+1)%channel_assignment.size()]),1);
    bcm2835_delayMicroseconds(5);

    SPI_assert_ADC();
    bcm2835_spi_transfer(CMD_SYNC);
    SPI_release_ADC();
    bcm2835_delayMicroseconds(5);

    SPI_assert_ADC();
    bcm2835_spi_transfer(CMD_WAKEUP);
    SPI_release_ADC();
    bcm2835_delayMicroseconds(25);

    SPI_assert_ADC();
    bcm2835_spi_transfer(CMD_RDATA);
    bcm2835_delayMicroseconds(10);

    bcm2835_spi_transfern(dummy_buf,3);
    SPI_release_ADC();
  }

  // correct channel is now staged for conversion
  sample_buffer_ptr sample_buffer;
  while(!done.load() && !trigger.should_stop()) {
    // get the next data_block
    if(!allocation_ringbuffer.pop(sample_buffer))
      continue;

    // cycling through the channels is done with a one cycle lag. That is,
    // while we are pulling the converted data off of the ADC's register, we
    // have already switched the conversion hardware to the next channel so that
    // off it can be settling down while we are in the process of pulling the
    // data for the previous conversion. See the datasheet pg. 21
    sample_buffer_type::pointer data_buffer = sample_buffer->data();

    std::size_t rows;
    for(rows=0; rows<row_block && !trigger.should_stop(); ++rows) {
      for(std::size_t chan=0; chan<channel_assignment.size(); ++chan) {
        // 2,083.3328 usec max wait
        wait_DRDY();

        // switch to the next channel
        write_to_registers(REG_MUX,
          &(channel_assignment[(chan+1)%channel_assignment.size()]),1);
        bcm2835_delayMicroseconds(5);

        SPI_assert_ADC();
        bcm2835_spi_transfer(CMD_SYNC);
        SPI_release_ADC();
        bcm2835_delayMicroseconds(5);

        SPI_assert_ADC();
        bcm2835_spi_transfer(CMD_WAKEUP);
        SPI_release_ADC();
        bcm2835_delayMicroseconds(25);

        SPI_assert_ADC();
        bcm2835_spi_transfer(CMD_RDATA);
        bcm2835_delayMicroseconds(10);

        bcm2835_spi_transfern(data_buffer,3);
        SPI_release_ADC();

        data_buffer += 3;
      }
    }

    ready_ringbuffer.push(sample_buffer);
  }

  done.store(true);
  servicing_thread.join();
}

void waveshare_ADS1256::trigger_sampling_async_wstat_impl(
  const data_handler &handler, basic_trigger &trigger)
{
  typedef std::chrono::high_resolution_clock::time_point time_point_type;
  typedef std::chrono::nanoseconds::rep nanosecond_rep_type;

  static const std::size_t time_size = sizeof(nanosecond_rep_type);

  static const std::size_t max_allocation = 32;

  ringbuffer_type allocation_ringbuffer(max_allocation);
  ringbuffer_type ready_ringbuffer(max_allocation);

  std::size_t buffer_size =
    row_block*channel_assignment.size()*(bit_depth()/8+time_size);

  for(std::size_t i=0; i<max_allocation; ++i)
    allocation_ringbuffer.push(
      sample_buffer_ptr(new sample_buffer_type(buffer_size)));

  std::atomic_int done(false);

  std::thread servicing_thread(&waveshare_ADS1256::async_handler,*this,
    std::ref(allocation_ringbuffer), std::ref(ready_ringbuffer),
    std::cref(handler), std::ref(done));

  trigger.wait_start();

  // cycle through once and throw away data to set per-channel statistics and
  // ensure valid data on first "real" sample
  char dummy_buf[3];
  for(std::size_t chan=0;
    chan<channel_assignment.size() && !trigger.should_stop();
    ++chan)
  {
    // 2,083.3328 usec max wait
    wait_DRDY();

    // switch to the next channel
    write_to_registers(REG_MUX,
      &(channel_assignment[(chan+1)%channel_assignment.size()]),1);
    bcm2835_delayMicroseconds(5);

    SPI_assert_ADC();
    bcm2835_spi_transfer(CMD_SYNC);
    SPI_release_ADC();
    bcm2835_delayMicroseconds(5);

    SPI_assert_ADC();
    bcm2835_spi_transfer(CMD_WAKEUP);
    SPI_release_ADC();
    bcm2835_delayMicroseconds(25);

    SPI_assert_ADC();
    bcm2835_spi_transfer(CMD_RDATA);
    bcm2835_delayMicroseconds(10);

    bcm2835_spi_transfern(dummy_buf,3);
    SPI_release_ADC();
  }

  time_point_type start_time = std::chrono::high_resolution_clock::now();

  // correct channel is now staged for conversion
  sample_buffer_ptr sample_buffer;
  while(!done.load() && !trigger.should_stop()) {
    // get the next data_block
    if(!allocation_ringbuffer.pop(sample_buffer))
      continue;

    // cycling through the channels is done with a one cycle lag. That is,
    // while we are pulling the converted data off of the ADC's register, we
    // have already switched the conversion hardware to the next channel so that
    // off it can be settling down while we are in the process of pulling the
    // data for the previous conversion. See the datasheet pg. 21
    sample_buffer_type::pointer data_buffer = sample_buffer->data();

    std::size_t rows;
    for(rows=0; rows<row_block && !trigger.should_stop(); ++rows) {
      for(std::size_t chan=0; chan<channel_assignment.size(); ++chan) {
        // 2,083.3328 usec max wait
        wait_DRDY();

        // switch to the next channel
        write_to_registers(REG_MUX,
          &(channel_assignment[(chan+1)%channel_assignment.size()]),1);
        bcm2835_delayMicroseconds(5);

        SPI_assert_ADC();
        bcm2835_spi_transfer(CMD_SYNC);
        SPI_release_ADC();
        bcm2835_delayMicroseconds(5);

        SPI_assert_ADC();
        bcm2835_spi_transfer(CMD_WAKEUP);
        SPI_release_ADC();
        bcm2835_delayMicroseconds(25);

        SPI_assert_ADC();
        bcm2835_spi_transfer(CMD_RDATA);
        bcm2835_delayMicroseconds(10);

        bcm2835_spi_transfern(data_buffer,3);
        SPI_release_ADC();

        std::chrono::high_resolution_clock::time_point now =
          std::chrono::high_resolution_clock::now();

        std::chrono::nanoseconds::rep elapsed =
          std::chrono::duration_cast<std::chrono::nanoseconds>(
            now-start_time).count();

        data_buffer += 3;
        elapsed = detail::ensure_be(elapsed);
        std::memcpy(data_buffer,&elapsed,time_size);
        data_buffer += time_size;
      }
    }

    ready_ringbuffer.push(sample_buffer);
  }

  done.store(true);
  servicing_thread.join();
}
#endif


}
