#include "AD9854.h"
#include "BigNumber.h"
#include <Energia.h>
#include <SPI.h>

static char controlRegister[4];
static char read_spi_data[6];
static char* KO_MSG = "KO";
static char* OK_MSG = "OK";
static char* NI_MSG = "NI";
static char* ZERO_MSG = "\x00";
static char* ONE_MSG = "\x01";
static char *MODULATION[6] = {"None         ", "FSK          ", "Ramped FSK   ", "Chirp        ", "BPSK         ", "Not Allowed  "};
 

 DDS::DDS(int CS, int UDCLK, int IO_RESET, int MRESET, int SPI_port, int SPI_delay)
{
	_dds_cs=CS;
	_dds_udclk=UDCLK;
	_dds_io_reset=IO_RESET;
	_dds_mreset=MRESET;
	_spi_device=SPI_port;
	_spi_delay=SPI_delay;
}

int DDS::init()
{
	SPI.setModule(1);
	SPI.setBitOrder(MSBFIRST);
	SPI.begin();

	pinMode(_dds_cs, OUTPUT);
	pinMode(_dds_udclk, OUTPUT);
	pinMode(_dds_io_reset, OUTPUT);
	pinMode(_dds_mreset, OUTPUT);

    _clock = 60000000;        			// Work clock in MHz

    _ctrlreg_multiplier = 4;        	// Multiplier 4- 20
    _ctrlreg_mode = 0;              	// Single, FSK, Ramped FSK, Chirp, BPSK
    
    _ctrlreg_qdac_pwdn = 0;         	// QDAC power down enabled: 0 -> disable
    _ctrlreg_ioupdclk = 0;          	// IO Update clock direction: 0 -> input,  1 -> output
    _ctrlreg_inv_sinc  = 0;         	// Sinc inverser filter enable: 0 -> enable
    _ctrlreg_osk_en = 1;            	// Enable Amplitude multiplier: 0 -> disabled
    _ctrlreg_osk_int = 0;           	// register/counter output shaped control: 0 -> register, 1 -> counter
    _ctrlreg_msb_lsb = 0;           	// msb/lsb bit first: 0 -> MSB, 1 -> LSB
    _ctrlreg_sdo = 1;               	// SDO pin active: 0 -> inactive
     
    reset();
   
    if (not writeControlRegister())
    {
      	isConfig = false;
        return false;
    }
        
   
    isConfig = true;
    
    return true;

}

int DDS::reset()
{
	on(_dds_mreset);
	delay(1);
	off(_dds_mreset);
	delay(1);

	return 1;
}

int DDS::io_reset()
{
  	on(_dds_io_reset);
  	delayMicroseconds(_spi_delay);
  	off(_dds_io_reset);
  	delayMicroseconds(_spi_delay);

	return 1;
}

void DDS::on(int x) {
  digitalWrite(x, HIGH);
}

void DDS::off(int x) {
  digitalWrite(x, LOW);
}

char* DDS::readData(char addr, char ndata)
{
    // I/O reset
    io_reset();

    off(_dds_cs);

    //Sending serial address
    SPI.transfer((addr & 0x0F) | 0x80);
    for(char i = 0; i < ndata; i++)
    {
        delayMicroseconds(_spi_delay);
        read_spi_data[i] =SPI.transfer(0x00);
    }
    
   	on(_dds_cs);
    
    return read_spi_data;
    
}

int DDS::writeData(char addr, char ndata, const char* data){

    off(_dds_udclk);
   	io_reset();
	off(_dds_cs);

    SPI.transfer(addr & 0x0F);

    for(char i = 0; i < ndata; i++)
    {
        delayMicroseconds(_spi_delay);
        SPI.transfer(data[i]);
    }

    on(_dds_cs);
    delayMicroseconds(10);
    on(_dds_udclk);
    delayMicroseconds(10);
    off(_dds_udclk);
    delayMicroseconds(10);
	
    return 1;
}

int DDS::writeControlRegister(){
	
    bool success;
    char* write_spi_data;
    char* read_spi_data;
    char addr = 0x07, ndata = 4;
	
    write_spi_data = getControlRegister();
	
    success = writeData(addr, ndata, write_spi_data);
   
    //dds_updclk->output();
	
    delayMicroseconds(100);
    on(_dds_udclk);
    delayMicroseconds(10);
    off(_dds_udclk);
    delayMicroseconds(10);
	
    read_spi_data = readData(addr, ndata);
	
    success = true;
	
    for(char i = 0; i < ndata; i++)
    {
        if (write_spi_data[i] != read_spi_data[i])
        {
            success = false;
            break;
        }
    }
	
    return success;
}   

char* DDS::getControlRegister()
{
    
    bool pll_range = 0;
    bool pll_bypass = 1;
    
    if (_ctrlreg_multiplier >= 4){
        pll_bypass = 0;
    }
 
    if (_clock >= 200){
        pll_range = 1;
    }
       
    controlRegister[0] = 0x10 + _ctrlreg_qdac_pwdn*4;
    controlRegister[1] = pll_range*64 + pll_bypass*32 + (_ctrlreg_multiplier & 0x1F);
    controlRegister[2] = (_ctrlreg_mode & 0x07)*2 + _ctrlreg_ioupdclk;
    controlRegister[3] = _ctrlreg_inv_sinc*64 + _ctrlreg_osk_int*32 + _ctrlreg_osk_int*16 + _ctrlreg_msb_lsb*2 + _ctrlreg_sdo;
    
    return controlRegister;
    
}
/*
########################################################################
########################################################################
########################################################################
*/

DDS_function::DDS_function()
{
	BigNumber::begin(); 
}

BigNumber DDS_function::pow64bits(int a, int b)
{
	BigNumber result = 1;
	BigNumber base = a;
	for (int i = 0; i < b; i++) 
	{
		result = result * base;
	}
	return result;
}

char* DDS_function::freq2binary(float freq, float mclock) 
{
	DDS_function _x;
	static char bytevalue[6] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

	BigNumber DesiredOut = freq;
	BigNumber SYSCLK = mclock;
	BigNumber a = 0;
	BigNumber b = 256; // 2 bytes=16 bits
	a = DesiredOut * _x.pow64bits(2, 48)/ SYSCLK;
	int n = 5;
	while (a != 0) 
	{
		bytevalue[n] =  byte(a % b);
		a =  a / b;
		n--;
	}
	return bytevalue;
}

void DDS_function::print(char* msg, char dim){

	int x=dim;
	Serial.print("[");
	for (int i=0; i<x-1;i++)
	{
		Serial.print(msg[i], HEX);
	 	Serial.print(", ");
	}
	Serial.print(msg[x-1], HEX);
	Serial.println("]");
}
	
 
// int DDS::__writeDataAndVerify(char addr, char ndata, const char* wr_spi_data, SerialDriver *screen){
	
//     int    success;
//     char*  rd_spi_data;
	
//     this->__writeData(addr, ndata, wr_spi_data);
//     rd_spi_data = this->__readData(addr, ndata);
	
//     success = 1;
	
//     for(char i = 0; i < ndata; i++)
//     {
//         if (screen != NULL){
//             screen->putc(wr_spi_data[i]);
//             screen->putc(0x3D);
//             screen->putc(rd_spi_data[i]);
//         }
		
//         if (wr_spi_data[i] != rd_spi_data[i])
//         {
//             success = 0;
//             break;
//         }
		
//     }
	
//     //Update Control Register
//     if ((success == 1) && (addr==0x07)){
//         cr_multiplier = rd_spi_data[1] & 0x1F;
//         cr_mode = (rd_spi_data[2] & 0x0E) >> 1;
//     }
//     //printf("\r\nSuccessful writting = %d\r\n", success);
	
//     return success;
// }
 
// char* DDS::__getControlRegister(){
	
//     bool pll_range = 0;
//     bool pll_bypass = 1;
	
//     if (cr_multiplier >= 4){
//         pll_bypass = 0;
//     }
 
//     if (clock >= 200){
//         pll_range = 1;
//     }
	   
//     controlRegister[0] = 0x10 + cr_qdac_pwdn*4;
//     controlRegister[1] = pll_range*64 + pll_bypass*32 + (cr_multiplier & 0x1F);
//     controlRegister[2] = (cr_mode & 0x07)*2 + cr_ioupdclk;
//     controlRegister[3] = cr_inv_sinc*64 + cr_osk_en*32 + cr_osk_int*16 + cr_msb_lsb*2 + cr_sdo;
	
//     return controlRegister;
	
//     }
	
				
// int DDS::reset(){
	
//     // Master reset
//     //Set as a input, temporary
//     //printf("\r\nChange updclk direction as an INPUT ...\r\n");
//     dds_updclk->input();
//     dds_updclk->mode(PullDown);
	
//     //printf("\r\nReseting DDS ...\r\n");
//     *dds_mreset = 1;
//     wait_ms(1);
//     *dds_mreset = 0;
//     wait_ms(1);
	
//     this->rf_enabled = false;
	
//     return 0;
//     }
	
// int DDS::scanIOUpdate(){
	
//     unsigned int cont = 0;
	
//     this->reset();
	
//     //printf("\r\nWaiting a upd_clk ...\r\n");
//     while(true){
//         if (*dds_updclk == 1)
//             break;
		
//         cont += 1;
//         if (cont > 10000)
//             break;
			
//         wait_us(1);
//     }
	
//     if (cont > 10000){
//         //printf("\r\nA upd_clk was not found\r\n");
//         return 0;
//     }
	
//     //printf("\r\nA upd_clk was found ...\r\n");
	
//     return 1;
//     }
	
// int DDS::find(){
//     /*
//     char phase[];
	
//     phase[0] = 0x0A;
//     phase[1] = 0x55;
	
//     this->__writeDataAndVerify(0x00, 5, phase);
//     */
//     this->__readData(0x05, 4);
//     this->__readData(0x0A, 1);
//     return 1;
	
//     }
	
	
// int DDS::init(){
	
//     //printf("\r\nSetting default parameters in CR ...\r\n");
	
//     //Serial mode enabled
//     this->clock = 200.0;        // Work clock in MHz
//     this->cr_multiplier = 4;        // Multiplier 4- 20
//     this->cr_mode = 0;              // Single, FSK, Ramped FSK, Chirp, BPSK
//     this->cr_qdac_pwdn = 0;         // QDAC power down enabled: 0 -> disable
//     this->cr_ioupdclk = 0;          // IO Update clock direction: 0 -> input,  1 -> output
//     this->cr_inv_sinc  = 0;         // Sinc inverser filter enable: 0 -> enable
//     this->cr_osk_en = 1;            // Enable Amplitude multiplier: 0 -> disabled
//     this->cr_osk_int = 0;           // register/counter output shaped control: 0 -> register, 1 -> counter
//     this->cr_msb_lsb = 0;           // msb/lsb bit first: 0 -> MSB, 1 -> LSB
//     this->cr_sdo = 1;               // SDO pin active: 0 -> inactive
 
//     //printf("\r\nSetting in serial mode ...\r\n");
//     *dds_sp_mode = 0;
//     *dds_cs = 1;
	 
//     this->reset();
	
//     //printf("\r\nWritting CR ...\r\n");
	
//     if (not this->__writeControlRegister()){
//         //printf("\r\nUnsuccessful DDS initialization");
//         this->isConfig = false;
//         return false;
//         }
		
//     //printf("\r\nSuccessfull DDS initialization");
	
//     this->isConfig = true;
	
//     return true;
// }
 
// char* DDS::rdMode(){
	
//     char* rd_data;
//     char mode;
	
//     rd_data = this->__readData(0x07, 4);
//     mode = (rd_data[2] & 0x0E) >> 1;
	
//     this->cr_mode = mode;
	
//     rd_data[0] = mode;
	
//     return rd_data;
//     }
	
// char* DDS::rdMultiplier(){
	
//     char* rd_data;
//     char mult;
	
//     rd_data = this->__readData(0x07, 4);
//     mult = (rd_data[1] & 0x1F);
//     this->cr_multiplier = mult;
	
//     //Reaconditioning data to return
//     rd_data[0] = mult;
//     rd_data[1] = ((int)clock >> 8) & 0xff; 
//     rd_data[2] = (int)clock & 0xff; 
	
//     return rd_data;    
//     }
// char* DDS::rdPhase1(){
 
//     char* rd_data;
	
//     rd_data = this->__readData(0x00, 2);
	
//     return rd_data;
	
//     }
// char* DDS::rdPhase2(){
 
//     char* rd_data;
	
//     rd_data = this->__readData(0x01, 2);
	
//     return rd_data;
//     }
// char* DDS::rdFrequency1(){
 
//     char* rd_data;
	
//     rd_data = this->__readData(0x02, 6);
	
//     for (int i=0; i<6; i++)
//         frequency1[i] = rd_data[i];
	
//     return rd_data;
	
//     }
// char* DDS::rdFrequency2(){
 
//     char* rd_data;
	
//     rd_data = this->__readData(0x03, 6);
	
//     for (int i=0; i<6; i++)
//         frequency2[i] = rd_data[i];
		
//     return rd_data;
//     }
// char* DDS::rdAmplitudeI(){
 
//     char* rd_data;
	
//     rd_data = this->__readData(0x08, 2);
	
//     return rd_data;
//     }
// char* DDS::rdAmplitudeQ(){
 
//     char* rd_data;
	
//     rd_data = this->__readData(0x09, 2);
	
//     return rd_data;
//     }
 
// int DDS::isRFEnabled(){
	
//     if (this->rf_enabled)
//         return 1;
	
//     return 0;
//     }
	
// int DDS::wrMode(char mode){
	
//     this->cr_mode = mode & 0x07;
	
//     return this->__writeControlRegister();
//     }
 
// int DDS::wrMultiplier(char multiplier, float clock){
	
//     this->cr_multiplier = multiplier & 0x1F;
//     this->clock = clock;
	
//     //printf("\r\n mult = %d, clock = %f", multiplier, clock);
//     //printf("\r\n cr_mult = %d", cr_multiplier);
	
//     return this->__writeControlRegister();
//     }
		
// int DDS::wrPhase1(char* phase, SerialDriver *screen){
	
//     return this->__writeDataAndVerify(0x00, 2, phase, screen);
	
//     }
	
// int DDS::wrPhase2(char* phase, SerialDriver *screen){
	
//     return this->__writeDataAndVerify(0x01, 2, phase, screen);
	
//     }
	
// int DDS::wrFrequency1(char* freq, SerialDriver *screen){
//     int sts;
	
//     sts =  this->__writeDataAndVerify(0x02, 6, freq, screen);
	
//     if (sts){
//         for (int i=0; i<6; i++)
//             frequency1[i] = freq[i];
//     }
//     return sts;
	
//     }
// int DDS::wrFrequency2(char* freq, SerialDriver *screen){
//     int sts;
	
//     sts = this->__writeDataAndVerify(0x03, 6, freq, screen);
	
//     if (sts){
//         for (int i=0; i<6; i++)
//             frequency2[i] = freq[i];
//     }
//     return sts;
//     }
 
// int DDS::wrAmplitudeI(char* amplitude, SerialDriver *screen){
	
//     amplitudeI[0] = amplitude[0];
//     amplitudeI[1] = amplitude[1];
	
//     this->rf_enabled = true;
	
//     return this->__writeDataAndVerify(0x08, 2, amplitude, screen);
	
//     }
 
// int DDS::wrAmplitudeQ(char* amplitude, SerialDriver *screen){
	
//     amplitudeQ[0] = amplitude[0];
//     amplitudeQ[1] = amplitude[1];
	 
//     this->rf_enabled = true;
	
//     return this->__writeDataAndVerify(0x09, 2, amplitude, screen);
	
//     }
 
// int DDS::enableRF(){
	
//     this->rf_enabled = true;
	
//     this->__writeDataAndVerify(0x08, 2, this->amplitudeI);
//     return this->__writeDataAndVerify(0x09, 2, this->amplitudeQ);
 
//     }
 
// int DDS::disableRF(){
	
//     this->rf_enabled = false;
	
//     this->__writeDataAndVerify(0x08, 2, "\x00\x00");
//     return this->__writeDataAndVerify(0x09, 2, "\x00\x00");
	
//     }
	   
// int DDS::defaultSettings(SerialDriver *screen){
	
//     if (!(screen == NULL)){
//         screen->putc(0x37);
//         screen->putc(0x30);
//     }
	
//     this->wrMultiplier(1, 0.0);
//     this->wrAmplitudeI("\x0F\xC0", screen);                //0xFC0 produces best SFDR than 0xFFF
//     this->wrAmplitudeQ("\x0F\xC0");                        //0xFC0 produces best SFDR than 0xFFF    
//     this->wrFrequency1("\x00\x00\x00\x00\x00\x00");        // 49.92 <> 0x3f 0xe5 0xc9 0x1d 0x14 0xe3 <> 49.92/clock*(2**48) \x3f\xe5\xc9\x1d\x14\xe3
//     this->wrFrequency2("\x00\x00\x00\x00\x00\x00");
//     this->wrPhase1("\x00\x00");                            //0 grados
//     this->wrPhase2("\x20\x00");                            //180 grados <> 0x20 0x00 <> 180/360*(2**14)
//     this->disableRF();
		
//     if (!(screen == NULL)){
//         screen->putc(0x37);
//         screen->putc(0x31);
//     }
	
//     return this->wrMode(4);                                //BPSK mode
	
//     }
	
// char* DDS::setCommand(unsigned short cmd, char* payload, unsigned long payload_len){
	
//     bool success = false;    
//     char* tx_msg;
//     unsigned long tx_msg_len;
	
//     tx_msg = KO_MSG;
//     tx_msg_len = 2;
	
//     //printf("cmd = %d, payload_len = %d", cmd, payload_len);
 
//     //printf("\r\nPayload = ");
//     //for(unsigned long i=0; i< payload_len; i++)
//         //printf("0x%x ", payload[i]);
	
//     //Si el DDS no esta inicializado siempre retornar NI_MSG
//     if (not this->isConfig){
//         this->cmd_answer = NI_MSG;
//         this->cmd_answer_len = 2;
		
//         return this->cmd_answer;
//     }
	
//     switch ( cmd )
//       {
//         case DDS_CMD_RESET:
//             success = this->init();
//             break;
			
//         case DDS_CMD_ENABLE_RF:
//             if (payload_len == 1){
//                 if (payload[0] == 0)
//                     success = this->disableRF();
//                 else
//                     success = this->enableRF();
//             }
//             break;
			
//         case DDS_CMD_MULTIPLIER:
//             if (payload_len == 1){
//                 success = this->wrMultiplier(payload[0]);
//             }
//             if (payload_len == 3){
//                 unsigned short clock = payload[1]*256 + payload[2];
//                 success = this->wrMultiplier(payload[0], (float)clock);
//             }
//             break;
			
//         case DDS_CMD_MODE:
//             if (payload_len == 1){
//                 success = this->wrMode(payload[0]);
//             }
//             break;
			
//         case DDS_CMD_FREQUENCYA:
//             if (payload_len == 6){
//                 success = this->wrFrequency1(payload);
//             }
//             break;
			
//         case DDS_CMD_FREQUENCYB:
//             if (payload_len == 6){
//                 success = this->wrFrequency2(payload);
//             }
//             break;
			
//         case DDS_CMD_PHASEA:
//             if (payload_len == 2){
//                 success = this->wrPhase1(payload);
//             }
//             break;
			
//         case DDS_CMD_PHASEB:
//             if (payload_len == 2){
//                 success = this->wrPhase2(payload);
//             }
//             break;
 
//         case DDS_CMD_AMPLITUDE1:
//             if (payload_len == 2){
//                 success = this->wrAmplitudeI(payload);
//             }
//             break;
 
//         case DDS_CMD_AMPLITUDE2:
//             if (payload_len == 2){
//                 success = this->wrAmplitudeQ(payload);
//             }
//             break;
 
//         case DDS_CMD_READ | DDS_CMD_ENABLE_RF:
//             if (this->isRFEnabled() == 1)
//                 tx_msg = ONE_MSG;
//             else
//                 tx_msg = ZERO_MSG;
				
//             tx_msg_len = 1;
			
//             break;
			
//         case DDS_CMD_READ | DDS_CMD_MULTIPLIER:
//             tx_msg = this->rdMultiplier();
//             tx_msg_len = 1;
//             break;
			
//         case DDS_CMD_READ | DDS_CMD_MODE:
//             tx_msg = this->rdMode();
//             tx_msg_len = 1;
//             break;
			
//         case DDS_CMD_READ | DDS_CMD_FREQUENCYA:
//             tx_msg = this->rdFrequency1();
//             tx_msg_len = 6;
//             break;
			
//         case DDS_CMD_READ | DDS_CMD_FREQUENCYB:
//             tx_msg = this->rdFrequency2();
//             tx_msg_len = 6;
//             break;
			
//         case DDS_CMD_READ | DDS_CMD_PHASEA:
//             tx_msg = this->rdPhase1();
//             tx_msg_len = 2;
//             break;
			
//         case DDS_CMD_READ | DDS_CMD_PHASEB:
//             tx_msg = this->rdPhase2();
//             tx_msg_len = 2;
//             break;
 
//         case DDS_CMD_READ | DDS_CMD_AMPLITUDE1:
//             tx_msg = this->rdAmplitudeI();
//             tx_msg_len = 2;
//             break;
 
//         case DDS_CMD_READ | DDS_CMD_AMPLITUDE2:
//             tx_msg = this->rdAmplitudeQ();
//             tx_msg_len = 2;
//             break;
			
//         default:
//             success = false;
		
//       }
	
//     if (success){
//         tx_msg = OK_MSG;
//         tx_msg_len = 2;
//         }
	
//     this->cmd_answer = tx_msg;
//     this->cmd_answer_len = tx_msg_len;
	
//     return tx_msg;
// }
 
// char* DDS::getCmdAnswer(){
	
//     return this->cmd_answer;
	
//     }
	
// unsigned long DDS::getCmdAnswerLen(){
	
//     return this->cmd_answer_len;
	
//     }
 
// int DDS::setAllDevice(char* payload, SerialDriver *screen){
	
//     int sts;
//     char* phase1, *phase2;
//     char* freq1, *freq2;
//     char* delta_freq, *upd_rate_clk, *ramp_rate_clk;
//     char* control_reg;
//     char* amplitudeI, *amplitudeQ, *ampl_ramp_rate;
//     char* qdac;
	
//     phase1 = &payload[0x00];
//     phase2 = &payload[0x02];
//     freq1 = &payload[0x04];
//     freq2 = &payload[0x0A];
//     delta_freq = &payload[0x10];
//     upd_rate_clk = &payload[0x16];
//     ramp_rate_clk = &payload[0x1A];
//     control_reg = &payload[0x1D];
//     amplitudeI = &payload[0x21];
//     amplitudeQ = &payload[0x23];
//     ampl_ramp_rate = &payload[0x25];
//     qdac = &payload[0x26];
	
//     control_reg[2] = control_reg[2] & 0xFE;     //cr_ioupdclk always as an input = 0
//     control_reg[3] = control_reg[3] & 0xFD;     //LSB first = 0, MSB first enabled
//     control_reg[3] = control_reg[3] | 0x01;     //cr_sdo enable = 1
	
//     this->__writeDataAndVerify(0x04, 6, delta_freq);
//     this->__writeDataAndVerify(0x05, 4, upd_rate_clk);
//     this->__writeDataAndVerify(0x06, 3, ramp_rate_clk);
//     this->__writeDataAndVerify(0x07, 4, control_reg);
	
//     this->__writeDataAndVerify(0x0A, 1, ampl_ramp_rate);
//     this->__writeDataAndVerify(0x0B, 2, qdac, screen);  
 
//     this->wrPhase1(phase1);
//     this->wrPhase2(phase2);
//     this->wrFrequency1(freq1);
//     this->wrFrequency2(freq2);
//     this->wrAmplitudeI(amplitudeI);
//     this->wrAmplitudeQ(amplitudeQ);
	
//     //Enabling RF
//     sts = this->enableRF();
	
//     return sts;
	
//     }
 
// bool DDS::wasInitialized(){
	
//     return this->isConfig;
// }
 
// char DDS::getMultiplier(){
//     return this->cr_multiplier;
// }
 
// double DDS::getFreqFactor1(){
//     factor_freq1 = ((double)frequency1[0])/256.0 + ((double)frequency1[1])/65536.0  + ((double)frequency1[2])/16777216.0 + ((double)frequency1[3])/4294967296.0;
//     factor_freq1 *= ((double)this->cr_multiplier);
	
//     return factor_freq1;
// }
 
// double DDS::getFreqFactor2(){
//     factor_freq2 = ((double)frequency2[0])/256.0 + ((double)frequency2[1])/65536.0  + ((double)frequency2[2])/16777216.0 + ((double)frequency2[3])/4294967296.0;
//     factor_freq2 *= ((double)this->cr_multiplier);
	
//     return factor_freq2;
// }
 
// char DDS::getMode(){
//     return this->cr_mode;   
// }
 
// char* DDS::getModeStr(){
	
//     if (this->cr_mode > 4)
	
//     return MODULATION[this->cr_mode];   
// }
