#include "AD9854.h"
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
 

 DDS::DDS( float clock1, int CS, int UDCLK, int IO_RESET, int MRESET)
{
	_dds_cs=CS;
	_dds_udclk=UDCLK;
	_dds_io_reset=IO_RESET;
	_dds_mreset=MRESET;
	_spi_delay=150;
    _clock=clock1;

    pinMode(_power, OUTPUT);

    pinMode(_dds_cs, OUTPUT);
    pinMode(_dds_udclk, OUTPUT);
    pinMode(_dds_io_reset, OUTPUT);
    pinMode(_dds_mreset, OUTPUT);

}

int DDS::init()
{

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
      	_isConfig = false;
        return false;
    }

    _isConfig = true;

    return true;

}



int DDS::reset()
{
	on(_dds_mreset);
	delay(1);
	off(_dds_mreset);
	delay(1);
    _rf_enabled=false;
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

void DDS::on(int x) 
{
  digitalWrite(x, HIGH);
}

void DDS::off(int x) 
{
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

int  DDS::verifyconnection()
{
    char* rd_spi_data;
    delayMicroseconds(10);
    rd_spi_data = readData(0x07,4);
    delayMicroseconds(10);
    int success = 1;
    print(rd_spi_data,4);
    if(rd_spi_data[0]==0 & rd_spi_data[1]==0 & rd_spi_data[2]==0 & rd_spi_data[3]==0)  
        success=0;
    else if (rd_spi_data[0]==0xFF & rd_spi_data[1]==0xFF & rd_spi_data[2]==0xFF & rd_spi_data[3]==0xFF)
        success=0;
    return success;
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

int DDS::writeDataAndVerify(char addr, char ndata, const char* wr_spi_data ){
	
    int    success;
    char*  rd_spi_data;
	
    writeData(addr, ndata, wr_spi_data);
    rd_spi_data = readData(addr, ndata);
	
    success = 1;
	
    for(char i = 0; i < ndata; i++)
    {
        if (wr_spi_data[i] != rd_spi_data[i])
        {
            success = 0;
            break;
        }
		
    }

	//Update Control Register
    if ((success == 1) && (addr==0x07)){
        _ctrlreg_multiplier = rd_spi_data[1] & 0x1F;
        _ctrlreg_mode = (rd_spi_data[2] & 0x0E) >> 1;
    }
	
	return success;
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
    
    if (_ctrlreg_multiplier >= 4 && _ctrlreg_multiplier<=12){
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

char* DDS::rdMode()
{
	
    char* rd_data;
    char* rd_mode;
    char mode;
	
    rd_data = readData(0x07, 4);
    mode = (rd_data[2] & 0x0E) >> 1;
    rd_mode[0] = mode;
	
    return rd_mode;
}
	
char* DDS::rdMultiplier()
{
	
    char* rd_data;
    char* rd_multiplier;
    char mult;
	
    rd_data = readData(0x07, 4);
    mult = (rd_data[1] & 0x1F);
    rd_multiplier[0]=mult;
    
    return rd_multiplier;    
}

char* DDS::rdPhase1()
{

    char* rd_data;
    rd_data = readData(0x00, 2);
    return rd_data;
}

char* DDS::rdPhase2()
{
 
    char* rd_data;
    rd_data = readData(0x01, 2);
    return rd_data;
}

char* DDS::rdControl()
{
 
    char* rd_data;
    rd_data = readData(0x87, 4);
    for (int i=0; i<4; i++) 
        _frequency1[i] = rd_data[i];

    return rd_data;
}

char* DDS::rdFrequency1()
{
 
    char* rd_data;
    rd_data = readData(0x02, 6);
    for (int i=0; i<6; i++)	
    	_frequency1[i] = rd_data[i];

    return rd_data;
	
}

char* DDS::rdFrequency2()
{

    char* rd_data;
    rd_data = readData(0x03, 6);
    for (int i=0; i<6; i++)
        _frequency2[i] = rd_data[i];

    return rd_data;
}

///////////////////////

char* DDS::rdAmplitudeI()
{

    char* rd_data;
    rd_data = readData(0x08, 2);
	
    return rd_data;
}

char* DDS::rdAmplitudeQ()
{

    char* rd_data;
    rd_data = readData(0x09, 2);
    return rd_data;
}
 
int DDS::isRFEnabled()
{
	
    if (_rf_enabled)
        return 1;
	
    return 0;
}
	
int DDS::wrMode(char mode)
{
	
    _ctrlreg_mode= mode & 0x07;
	
    return writeControlRegister();
}
 
int DDS::wrMultiplier(char multiplier, float clock)
{
    _ctrlreg_multiplier = multiplier & 0x1F;
    _clock = clock;
    return writeControlRegister();
}
		
int DDS::wrPhase1(char* phase)
{

    return writeDataAndVerify(0x00, 2, phase);
}
	
int DDS::wrPhase2(char* phase)
{
	
    return writeDataAndVerify(0x01, 2, phase);
}
	
int DDS::wrFrequency1(char* freq)
{

    int sts;
    sts =  writeDataAndVerify(0x02, 6, freq);
	
	if (sts){
        for (int i=0; i<6; i++)
            _frequency1[i] = freq[i];
    }

    return sts;
}

int DDS::wrFrequency2(char* freq)
{

    int sts;
    sts = writeDataAndVerify(0x03, 6, freq);
	
	if (sts){
        for (int i=0; i<6; i++)
            _frequency2[i] = freq[i];
    }

    return sts;
}

int DDS::wrAmplitudeI(char* amplitude)
{
	
    _amplitudeI[0] = amplitude[0];
    _amplitudeI[1] = amplitude[1];
	
    _rf_enabled = true;
	
    return writeDataAndVerify(0x08, 2, amplitude);
}
 
int DDS::wrAmplitudeQ(char* amplitude)
{
	
    _amplitudeQ[0] = amplitude[0];
    _amplitudeQ[1] = amplitude[1];
	 
    _rf_enabled = true;
	
    return writeDataAndVerify(0x09, 2, _amplitudeQ);
}
 
int DDS::enableRF()
{
	
    _rf_enabled = true;
    writeDataAndVerify(0x08, 2, _amplitudeI);

    return writeDataAndVerify(0x09, 2, _amplitudeQ);
}
 
int DDS::disableRF()
{
    
    _rf_enabled = false;
    writeDataAndVerify(0x08, 2, "\x00\x00");

    return writeDataAndVerify(0x09, 2, "\x00\x00");
	
}
	   
int DDS::defaultSettings()
{

    wrMultiplier(1, 0.0);
    wrAmplitudeI("\x0F\xC0");                //0xFC0 produces best SFDR than 0xFFF
    wrAmplitudeQ("\x0F\xC0");                        //0xFC0 produces best SFDR than 0xFFF    
    wrFrequency1("\x00\x00\x00\x00\x00\x00");        // 49.92 <> 0x3f 0xe5 0xc9 0x1d 0x14 0xe3 <> 49.92/clock*(2**48) \x3f\xe5\xc9\x1d\x14\xe3
    wrFrequency2("\x00\x00\x00\x00\x00\x00");
    wrPhase1("\x00\x00");                            //0 grados
    wrPhase2("\x20\x00");                            //180 grados <> 0x20 0x00 <> 180/360*(2**14)
    disableRF();
		
	return wrMode(4);                                //BPSK mode
	
}


bool DDS::wasInitialized()
{
    
    return _isConfig;
}
 
char DDS::getMultiplier()
{
    if(_ctrlreg_multiplier<4 || _ctrlreg_multiplier >12)
        _ctrlreg_multiplier=1;

    return _ctrlreg_multiplier;
}
 
double DDS::getFreqFactor1()
{

    _factor_freq1 = ((double)_frequency1[0])/256.0 + ((double)_frequency1[1])/65536.0  + ((double)_frequency1[2])/16777216.0 + ((double)_frequency1[3])/4294967296.0;
    _factor_freq1 *= ((double)_ctrlreg_multiplier);
    
    return _factor_freq1;
}
 
double DDS::getFreqFactor2(){
    _factor_freq2 = ((double)_frequency2[0])/256.0 + ((double)_frequency2[1])/65536.0  + ((double)_frequency2[2])/16777216.0 + ((double)_frequency2[3])/4294967296.0;
    _factor_freq2 *= ((double)_ctrlreg_multiplier);
    
    return _factor_freq2;
}
 
char DDS::getMode()
{

    return _ctrlreg_mode;   
}
 
char* DDS::getModeStr()
{
    
    if (_ctrlreg_mode > 4)
    
    return MODULATION[_ctrlreg_mode];   
}

/*
########################################################################
########################################################################
########################################################################
*/


float DDS::getclock()
{   
    float clock=float(_clock);
    return clock;
}


char* DDS::freq2binary(float freq) 
{
    static char bytevalue[6] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

    double DesiredOut = freq, SYSCLK = _clock;
    double a = 0, b = 256; // 2 bytes=16 bits
    a = DesiredOut * pow(2, 48)/ SYSCLK;
    
    int n = 5;
    
    while (n >= 0) 
    {
        double x =floor(a/b);
        bytevalue[n] = byte(a-b*x) ;
        a=floor(a/b);
        n--;
    }
    return bytevalue;
}


void DDS::print(char* msg, char dim)
{

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

double DDS::binary2freq(char* fb) 
{
    double freq_number=0, value=0;
    value=binary2decimal(fb);
    freq_number=_clock*value/pow(2,48);
    return freq_number;
}

double DDS::binary2decimal(char* fb) 
{
    double value=0;
    value= float(fb[0])*pow(2,40)+float(fb[1])*pow(2, 32)+float(fb[2])*pow(2, 24)+float(fb[3])*pow(2, 16)+float(fb[4])*pow(2, 8)+float(fb[5])*1;
    return value;
}



 /*
########################################################################
########################################################################
########################################################################
*/

 
				
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
 
