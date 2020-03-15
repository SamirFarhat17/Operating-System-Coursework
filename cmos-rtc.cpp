/*
 * CMOS Real-time Clock
 * SKELETON IMPLEMENTATION -- TO BE FILLED IN FOR TASK (3)
 */

/*
 * STUDENT NUMBER: s1746788
 */
#include <infos/drivers/timer/rtc.h>
#include <infos/util/lock.h>
#include <arch/x86/pio.h>

using namespace infos::drivers;
using namespace infos::drivers::timer;
using namespace infos::util;
using namespace infos::arch::x86;

class CMOSRTC : public RTC {
public:
	static const DeviceClass CMOSRTCDeviceClass;

	const DeviceClass& device_class() const override
	{
		return CMOSRTCDeviceClass;
	}

	/**
	 * Interrogates the RTC to read the current date & time.
	 * @param tp Populates the tp structure with the current data & time, as
	 * given by the CMOS RTC device.
	 */
	void read_timepoint(RTCTimePoint& tp) override
	{
		tp = retrieve_timepoint();
		RTCTimePoint old_tp = tp;

		// Check timepoints till we get two different values (this avoids randomnes)
		while(!timepoint_equivalent(tp, old_tp)) {
			old_tp = tp;
			tp = retrieve_timepoint();
		}

		// Check if binary coded decimal. if so convert to binary
		if(check_bcd()) convert_binary(tp); 

		// Make adjustments if in 12 hr mode and currently PM
		if(in_12hr_mode() && (tp.hours & 0x80)) tp.hours = ((tp.hours & 0x7F) + 12) % 24;
	}

	/*
	 * @param first First Timepoint
	 * @param second Second Timepoint
	 * @return true if both timepoints are equivalent, false otherwise
	 */
	bool timepoint_equivalent(RTCTimePoint first, RTCTimePoint second) {
		if((first.seconds == second.seconds) && (first.minutes == second.minutes) && (first.hours == second.hours) && 
		(first.day_of_month == second.day_of_month) && (first.month == second.month) && (first.year == second.year)) {
			return true;
		}
		return false;
	}

	/**
	 * Converts a timepoint from BCD to binary values
	 * @param tp The timepoint to convert
	 */
	void convert_binary(RTCTimePoint& tp)
	{
		// Conversion as per the osdev CMOS pages
		tp.seconds = ((tp.seconds >> 4) * 10) + (tp.seconds & 0xF);
		tp.minutes = ((tp.minutes >> 4) * 10) + (tp.minutes & 0xF);
		tp.hours = ((tp.hours >> 4) * 10) + (tp.hours & 0xF);
		tp.day_of_month = ((tp.day_of_month >> 4) * 10) + (tp.day_of_month & 0xF);
		tp.month = ((tp.month >> 4) * 10) + (tp.month & 0xF);
		tp.year = ((tp.year >> 4) * 10) + (tp.year & 0xF);
	}

	/**
	 * Reads the CMOS to check if value is in BCD
	 * @returns true if we in Binary Coded Decimal
	 */
	bool check_bcd() {
		// We need to prevent interrupts when accessing the CMOS
		UniqueIRQLock lock;
		return (get_bit(0xB, 2) == 0);
	}

	bool in_12hr_mode()
	{
		// Diable interrupts when accessing RTC
		UniqueIRQLock l;

		// Read bit 1 from status register B
		return get_bit(0xB, 2) == 0;
	}

	/**
	 * Reads a register from the CMOS
	 * @param reg The register to be read read
	 * @return The data at that register
	 */
	uint8_t get_register(int reg)
	{
		__outb(0x70, reg);
		return __inb(0x71);
	}

	/**
	 * Reads a specific bit from a register in the CMOS
	 * @param reg The register 
	 * @param bit The nth bit of the register to be read
	 * @return The nth bit at the given register
	 */
	uint8_t get_bit(int reg, int bit)
	{
		return (get_register(reg) >> bit) & 1;
	}

	/**
	 * Reads a timepoint from the RTC.
	 * If an RTC update is in progress, it will block until it completes.
	 * @return The timepoint composed from several register reads
	 */
	RTCTimePoint retrieve_timepoint() {
		// Make sure interrupts aren't allowed when accessing the RTC
		UniqueIRQLock lock;

		while(get_bit(0xA, 7) != 0) {
			// Wait for completion by checking the status continually
		}

		// collect relevant data and return
		RTCTimePoint tp = {
			.seconds = get_register(0x00),
			.minutes = get_register(0x02),
			.hours = get_register(0x04),
			.day_of_month = get_register(0x07),
			.month = get_register(0x08),
			.year = get_register(0x09),
		};

		return tp;
	}
};

const DeviceClass CMOSRTC::CMOSRTCDeviceClass(RTC::RTCDeviceClass, "cmos-rtc");
RegisterDevice(CMOSRTC);