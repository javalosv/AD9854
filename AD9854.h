#ifndef        AD9854_ISP_DRIVER
#define        AD9854_ISP_DRIVER

#include "Energia.h"
#include <SPI.h>


// #define SPI_BITS 8
// #define SPI_MODE 0
// #define SPI_FREQ 1000000
 
// #define DDS_CMD_RESET       0X10
// #define DDS_CMD_ENABLE_RF   0x11
// #define DDS_CMD_MULTIPLIER  0X12
// #define DDS_CMD_MODE        0x13
// #define DDS_CMD_FREQUENCYA  0X14
// #define DDS_CMD_FREQUENCYB  0x15
// #define DDS_CMD_PHASEA      0X16
// #define DDS_CMD_PHASEB      0x17
// #define DDS_CMD_AMPLITUDE1  0X18
// #define DDS_CMD_AMPLITUDE2  0x19
// #define DDS_CMD_READ        0x8000
 
class DDS{
	private:
		double           _clock;              // Work frequency in MHz
		
		char            _ctrlreg_multiplier;      // Multiplier 4- 20
		char            _ctrlreg_mode;            // Single, FSK, Ramped FSK, Chirp, BPSK
		
		bool            _ctrlreg_qdac_pwdn;       // Q DAC power down enable: 0 -> disable
		bool            _ctrlreg_ioupdclk;        // IO Update clock enable: 0 -> input
		bool            _ctrlreg_inv_sinc;        // Inverse sinc filter enable: 0 -> enable
		bool            _ctrlreg_osk_en;          // Enable AM: 0 -> disabled
		bool            _ctrlreg_osk_int;       	// ext/int output shaped control: 0 -> external
		bool            _ctrlreg_msb_lsb;     	// msb/lsb bit first: 0 -> MSB
		bool            _ctrlreg_sdo;        		// SDO pin active: 0 -> inactive.
 
		char            _frequency1[6];
		char            _frequency2[6];
		char            _phase1[2];
		char            _phase2[2];       
		char            _amplitudeI[2];
		char            _amplitudeQ[2];
		bool            _rf_enabled;
	   
		double          _factor_freq1;
		double          _factor_freq2;
		
		//DDS I/O
		int 			_dds_cs;
		int 			_dds_udclk;
		int 			_dds_io_reset;
		int 			_dds_mreset;
		int 			_power;
		
		//SPI 
		int 			_spi_device;
		int 			_spi_delay;

			
		// char*           cmd_answer;
		// unsigned long   cmd_answer_len;
		
		
		
	public:
		DDS(float, int, int, int, int);
		
		int init();
		int reset();
		int io_reset();

		void on(int);
		void off(int);

		bool _isConfig;
		int  verifyconnection();
		int writeDataAndVerify(char addr, char ndata, const char* wr_spi_data);
		char* getControlRegister();
		int writeControlRegister();
		int writeData(char, char, const char*);
		char* readData(char, char);
		int scanIOUpdate();
		int find();
		char* rdMode();
		char* rdMultiplier();
		char* rdPhase1();
		char* rdPhase2();
		char* rdFrequency1();
		char* rdFrequency2();
		char* rdAmplitudeI();
		char* rdAmplitudeQ();
		float getclock();
		int isRFEnabled();
		int wrMode(char mode);
		int wrMultiplier(char multiplier, float clock=200.0);
		int wrPhase1(char* phase);
		int wrPhase2(char* phase);
		int wrFrequency1(char* freq);
		int wrFrequency2(char* freq);
		int wrAmplitudeI(char* amplitude);
		int wrAmplitudeQ(char* amplitude);
		int enableRF();
		int disableRF();
		int defaultSettings();
		char* setCommand(unsigned short cmd, char* payload, unsigned long payload_len);
		char* getCmdAnswer();
		unsigned long getCmdAnswerLen();
		int setAllDevice(char* payload);
		bool wasInitialized();
		char getMultiplier();
		double getFreqFactor1();
		double getFreqFactor2();
		char getMode();
		char* getModeStr();
		char* rdControl();
		double binary2decimal(char*);
		double binary2freq(char*) ;
		char* freq2binary(float );
		void print(char*, char);

};


#endif