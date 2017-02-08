#ifndef        AD9854_ISP_DRIVER
#define        AD9854_ISP_DRIVER

#include "BigNumber.h"


#define SPI_BITS 8
#define SPI_MODE 0
#define SPI_FREQ 1000000
 
#define DDS_CMD_RESET       0X10
#define DDS_CMD_ENABLE_RF   0x11
#define DDS_CMD_MULTIPLIER  0X12
#define DDS_CMD_MODE        0x13
#define DDS_CMD_FREQUENCYA  0X14
#define DDS_CMD_FREQUENCYB  0x15
#define DDS_CMD_PHASEA      0X16
#define DDS_CMD_PHASEB      0x17
#define DDS_CMD_AMPLITUDE1  0X18
#define DDS_CMD_AMPLITUDE2  0x19
#define DDS_CMD_READ        0x8000
 
class DDS{
	private:
		float           clock;              // Work frequency in MHz
		char            cr_multiplier;      // Multiplier 4- 20
		char            cr_mode;            // Single, FSK, Ramped FSK, Chirp, BPSK
		bool            cr_qdac_pwdn;       // Q DAC power down enable: 0 -> disable
		bool            cr_ioupdclk;        // IO Update clock enable: 0 -> input
		bool            cr_inv_sinc;        // Inverse sinc filter enable: 0 -> enable
		bool            cr_osk_en;          // Enable AM: 0 -> disabled
		bool            cr_osk_int;       	// ext/int output shaped control: 0 -> external
		bool            cr_msb_lsb;     	// msb/lsb bit first: 0 -> MSB
		bool            cr_sdo;        		// SDO pin active: 0 -> inactive.
 
		char            freq1[6];
		char            freq2[6];
		char            phase1[2];
		char            phase2[2];       
		char            amplitudeI[2];
		char            amplitudeQ[2];
		bool            rf_enabled;
	   
		double          factor_freq1;
		double          factor_freq2;
		
		//SPI             *spi_device;
		//DDS I/O
		// DigitalOut      *dds_mreset;
		// DigitalOut      *dds_outramp;
		// DigitalOut      *dds_sp_mode;
		// DigitalOut      *dds_cs;
		// DigitalOut      *dds_io_reset;
		// DigitalInOut    *dds_updclk;
		
		// char*           cmd_answer;
		// unsigned long   cmd_answer_len;
		
		// int __writeData(char addr, char ndata, const char* data);
		// char* __readData(char addr, char ndata);
		// int __writeDataAndVerify(char addr, char ndata, const char* wr_spi_data, SerialDriver *screen=NULL);
		// char* __getControlRegister();
		// int __writeControlRegister();
		
	public:
		bool isConfig;
 
		DDS();
		//DDS(SPI *spi_dev, DigitalOut *mreset, DigitalOut *outramp, DigitalOut *spmode, DigitalOut *cs, DigitalOut *ioreset, DigitalInOut *updclk);
		int init();
		void reset(int x, int y);
		// int reset();
		// int scanIOUpdate();
		// int find();
		// char* rdMode();
		// char* rdMultiplier();
		// char* rdPhase1();
		// char* rdPhase2();
		// char* rdFrequency1();
		// char* rdFrequency2();
		// char* rdAmplitudeI();
		// char* rdAmplitudeQ();
		// int isRFEnabled();
		// int wrMode(char mode);
		// int wrMultiplier(char multiplier, float clock=200.0);
		// int wrPhase1(char* phase, SerialDriver *screen=NULL);
		// int wrPhase2(char* phase, SerialDriver *screen=NULL);
		// int wrFrequency1(char* freq, SerialDriver *screen=NULL);
		// int wrFrequency2(char* freq, SerialDriver *screen=NULL);
		// int wrAmplitudeI(char* amplitude, SerialDriver *screen=NULL);
		// int wrAmplitudeQ(char* amplitude, SerialDriver *screen=NULL);
		// int enableRF();
		// int disableRF();
		// int defaultSettings(SerialDriver *screen=NULL);
		// char* setCommand(unsigned short cmd, char* payload, unsigned long payload_len);
		// char* getCmdAnswer();
		// unsigned long getCmdAnswerLen();
		// int setAllDevice(char* payload, SerialDriver *screen=NULL);
		// bool wasInitialized();
		// char getMultiplier();
		// double getFreqFactor1();
		// double getFreqFactor2();
		// char getMode();
		// char* getModeStr();
		
};

class DDS_function{
	public:
		DDS_function();
		BigNumber _pow64bits(int a, int b);
		byte *_freq2binary(float freq, float mclock);
};
#endif