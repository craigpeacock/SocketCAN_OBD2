
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <linux/can.h>
#include <linux/can/raw.h>


#define PID_EngineCoolantTemperature 	0x05

#define PID_IntakeAirTemperature 		0x0F
#define PID_ControlModuleVoltage 		0x42
#define PID_AbsoluteLoadValue			0x43

#define PID_ENGINE_LOAD					0x04
#define PID_PID_INTAKE_MAP				0x0B
#define PID_RPM 						0x0C
#define PID_SPEED 						0x0D
#define PID_MAF_FLOW					0x10
#define PID_THROTTLE					0x11
#define PID_AIR_FUEL_EQUIV_RATIO		0x44
#define PID_ENGINE_FUEL_RATE			0x5E


int Service01Response(int fd, int PID);
int SendCANFrame(int s, struct can_frame *frame);
int SendPHEVBatteryHealth(int s, int frame_number);

int main(int argc, char **argv)
{
	int s, i;
	int frame_number;
	int nbytes;
	struct sockaddr_can addr;
	struct ifreq ifr;
	struct can_frame frame;

	printf("OBD2 PHEV Demo\r\n");

	if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
		perror("Socket");
		return 1;
	}

	strcpy(ifr.ifr_name, "can0" );
	ioctl(s, SIOCGIFINDEX, &ifr);

	memset(&addr, 0, sizeof(addr));
	addr.can_family = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;

	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("Bind");
		return 1;
	}

	do {
	
		nbytes = read(s, &frame, sizeof(struct can_frame));
	
		if (nbytes < 0) {
			perror("Read");
			return 1;
		}
	
		printf("0x%03X [%d] ",frame.can_id, frame.can_dlc);
		for (i = 0; i < frame.can_dlc; i++)
			printf("%02X ",frame.data[i]);
		printf("\r\n");
	
		if (frame.can_id == 0x7DF) {
			printf("Received OBD query\r\n");
			if (frame.can_dlc >= 2) {
				switch (frame.data[1]) {
					case 1: 
						printf("Service 01: Show current data\r\n");
						Service01Response(s, frame.data[2]);
						break;
					case 9: 	
						printf("Service 09: Request vehicle information\r\n");
						//Service09Response(s, frame.data[2]);
						break;
					default:
						printf("Unknown service 0x%02X",frame.data[1]);
						break;
				}
			} else {
				printf("Error, frame too short. DLC = %d bytes.\r\n",frame.can_dlc);
			}
		}
		if (frame.can_id == 0x761) {
			//printf("Mitsubishi Outlander PHEV\r\n");
			if ((frame.data[1] == 0x21) && (frame.data[2] == 0x01)) {
				printf("Received request for Mitsubishi PHEV Battery Health\r\n");
				SendPHEVBatteryHealth(s,1);
				frame_number = 2;
			}
			if ((frame.data[0] & 0xF0) == 0x30) {
				printf("Mitsubishi PHEV Flow Control\r\n");
				// Handle flow control according to https://en.wikipedia.org/wiki/ISO_15765-2
				switch (frame.data[0] & 0x0F) {
					case 0x0:
						if (frame.data[1] == 0){
							// Continue to send remaining frames without flow control or delay
							SendPHEVBatteryHealth(s,2);
							SendPHEVBatteryHealth(s,3);
							SendPHEVBatteryHealth(s,4);
							SendPHEVBatteryHealth(s,5);
							SendPHEVBatteryHealth(s,6);
							SendPHEVBatteryHealth(s,7);
							SendPHEVBatteryHealth(s,8);
						} else{
							// frame.data[1] contains number of frames to send before waiting for next flow control frame
							// frame.data[2] contains separation time
							if (frame.data[1] == 1) {
								// Send next frame
								if (frame_number != 0) SendPHEVBatteryHealth(s,frame_number++);
								if (frame_number >= 9) frame_number = 0;
							}
							else {
								// Send multiple frames with a delay
								
								/* To Do, loop here */
								
								if (frame.data[2] < 127) {
									// Seperation time in mS	
									usleep(frame.data[2] * 1000);	
								}
							}
						}
						break;
					case 0x1:
						// Wait
						break;
					case 0x2:
						// Overflow, abort
						break;
					default:
						printf("Unknown Flow Control Flag\r\n");
						break;
				}
			}
		}
	} while(1);
		
	if (close(s) < 0) {
		perror("Close");
		return 1;
	}

	return 0;
}

int SendPHEVBatteryHealth(int s, int frame_number)
{
	struct can_frame frame;
	frame.can_id = 0x762;
	frame.can_dlc = 8;

	/* 
	// https://www.myoutlanderphev.com/forum/viewtopic.php?f=10&t=1796
	SOC (%) 60
	Batt Health (Ah) 34
	Current Charge (Ah) 20
	Charge Current (A) 9
	Battery Voltage 318.2
	BatMaxIN (kW) 39
	BatMaxOut (kW) 63.5
	OR Power Supply 13.4
	IGCT Power Supply 13.9
	Internal Resistance Mohm 0
	Power (kW) 28.638
	*/
	
	if (frame_number == 1) {
		printf("Sending Packet 1\r\n");
		// 762 Pkt 1: 10 37 61 01 82 83 0F 8B
		frame.data[0] = 0x10; 		// First Frame, ISO_15765-2 + high length
		frame.data[1] = 0x37; 		// Number of bytes (0x37 = 55 bytes)
		frame.data[2] = 0x61; 		// Custom Service/Mode (Same as query + 0x40) 
		frame.data[3] = 0x01;		// PID
		frame.data[4] = 0x82;		// Byte 0 = SoC %  (x/2)-5
		frame.data[5] = 0x83;		// Byte 1
		frame.data[6] = 0x0F;		// Byte 2
		frame.data[7] = 0x8B;		// Byte 3
		SendCANFrame(s, &frame);
	}  
	
	if (frame_number == 2) {
		//sleep(1);
		printf("Sending Packet 2\r\n");
		// 762 Pkt 2: 21 24 0F 88 03 0C 6E 52
		frame.data[0] = 0x21;		// Consecutive frame, Sequence 1
		frame.data[1] = 0x24;		// Byte 4
		frame.data[2] = 0x0F;		// Byte 5
		frame.data[3] = 0x88;		// Byte 6
		frame.data[4] = 0x03;		// Byte 7
		frame.data[5] = 0x0C;		// Byte 8 Battery Voltage (High)
		frame.data[6] = 0x6E;		// Byte 9 Battery Voltage (Low) / 10
		frame.data[7] = 0x52;		// Byte 10
		SendCANFrame(s, &frame);
	}
    
	if (frame_number == 3) {
		//sleep(1);
		printf("Sending Packet 3\r\n");
		// 762 Pkt 3: 22 03 4D 1C 01 99 00 00
		frame.data[0] = 0x22;		// Consecutive frame, Sequence 2
		frame.data[1] = 0x03;		// Byte 11
		frame.data[2] = 0x4D;		// Byte 12
		frame.data[3] = 0x1C;		// Byte 13
		frame.data[4] = 0x01;		// Byte 14 Charge Current (High)
		frame.data[5] = 0x99;		// Byte 15 Charge Current (Low)
		frame.data[6] = 0x00;		// Byte 16
		frame.data[7] = 0x00;		// Byte 17
		SendCANFrame(s, &frame);	
	}
	
	if (frame_number == 4) {
		//sleep(1);
		printf("Sending Packet 4\r\n");
		// 762 Pkt 4: 23 00 00 00 01 00 01 30
		frame.data[0] = 0x23;		// Consecutive frame, Sequence 3
		frame.data[1] = 0x00;		// Byte 18
		frame.data[2] = 0x00;		// Byte 19
		frame.data[3] = 0x00;		// Byte 20
		frame.data[4] = 0x01;		// Byte 21
		frame.data[5] = 0x00;		// Byte 22
		frame.data[6] = 0x01;		// Byte 23
		frame.data[7] = 0x30;		// Byte 24 
		SendCANFrame(s, &frame);	
	}
	
	if (frame_number == 5) {
		//sleep(1);
		printf("Sending Packet 5\r\n");
		// 762 Pkt 5: 24 0F 0F 01 54 00 CC 9C
		frame.data[0] = 0x24;		// Consecutive frame, Sequence 4
		frame.data[1] = 0x0F;		// Byte 25
		frame.data[2] = 0x0F;		// Byte 26 
		frame.data[3] = 0x01;		// Byte 27 Batt Health Ah (High)
		frame.data[4] = 0x54;		// Byte 28 Batt Health Ah (Low)
		frame.data[5] = 0x00;		// Byte 29 Current Charge Ah (High)
		frame.data[6] = 0xCC;		// Byte 30 Current Charge Ah (Low)
		frame.data[7] = 0x9C;		// Byte 31 BatMaxIN kW 39
		SendCANFrame(s, &frame);	
	}
	
	if (frame_number == 6) {
		//sleep(1);
		printf("Sending Packet 6\r\n");
		// 762 Pkt 6: 25 FE 00 03 0F 88 86 8B
		frame.data[0] = 0x25;		// Consecutive frame, Sequence 5
		frame.data[1] = 0xFE;		// Byte 32 BatMaxOut (kW) 63.5
		frame.data[2] = 0x00;		// Byte 33
		frame.data[3] = 0x03;		// Byte 34
		frame.data[4] = 0x0F;		// Byte 35
		frame.data[5] = 0x88;		// Byte 36
		frame.data[6] = 0x86;		// Byte 37 OR Power Supply 13.4
		frame.data[7] = 0x8B;		// Byte 38 IGCT Power Supply 13.9
		SendCANFrame(s, &frame);
	}
	
	if (frame_number == 7) {
		//sleep(1);
		printf("Sending Packet 7\r\n");
		// 762 Pkt 7: 26 64 00 00 00 00 00 00
		frame.data[0] = 0x26;		// Consecutive frame, Sequence 6
		frame.data[1] = 0x64;		// Byte 39
		frame.data[2] = 0x00;		// Byte 40
		frame.data[3] = 0x00;		// Byte 41
		frame.data[4] = 0x00;		// Byte 42
		frame.data[5] = 0x00;		// Byte 43
		frame.data[6] = 0x00;		// Byte 44
		frame.data[7] = 0x00;		// Byte 45
		SendCANFrame(s, &frame);
	}
	
	if (frame_number == 8) {
		//sleep(1);
		printf("Sending Packet 8\r\n");
		// 762 Pkt 8: 27 00 00 0F 8A 00 02 00
		frame.data[0] = 0x27;		// Consecutive frame, Sequence 7
		frame.data[1] = 0x00;		// Byte 46
		frame.data[2] = 0x00;		// Byte 47
		frame.data[3] = 0x0F;		// Byte 48
		frame.data[4] = 0x8A;		// Byte 49
		frame.data[5] = 0x00;		// Byte 50
		frame.data[6] = 0x02;		// Byte 51
		frame.data[7] = 0x00;		// Byte 52 Internal Resistance 
		SendCANFrame(s, &frame);
	}
}

int Service01Response(int s, int PID)
{
	struct can_frame frame;
	frame.can_id = 0x7E8;
	frame.can_dlc = 8;
	
	printf("Service 1 PID = 0x%02X\r\n",PID);
	
	frame.data[0] = 2;		// Number of additional bytes
	frame.data[1] = 0x41;	// Custom Service/Mode (Same as query + 0x40)
	frame.data[2] = PID;	// PID 
	
	switch (PID) {
//		case PID_SupportedPIDs:
//			printf("Sending bitmask of supported PIDs\r\n");
//			break;
		case PID_ENGINE_LOAD:
			printf("Sending Engine Load (Percent)\r\n");
			frame.data[3] = 20;		// Value (first byte) 7.8%
			SendCANFrame(s, &frame);
			break;
		case PID_PID_INTAKE_MAP:
			printf("Intake manifold absolute pressure (kPa)\r\n");
			frame.data[3] = 20;		// Value (first byte) - 20kPa
			SendCANFrame(s, &frame);
			break;			
		case PID_RPM:
			printf("Sending Engine RPM\r\n");
			frame.data[3] = 20;		// Value (first byte)
			frame.data[4] = 0;		// Value (second byte)
			SendCANFrame(s, &frame);
			break;
		case PID_SPEED:
			printf("Sending Vehicle Speed (km/h)\r\n");
			frame.data[3] = 20;		// Value (first byte) - 20km/h
			SendCANFrame(s, &frame);
			break;
		case PID_MAF_FLOW:
			printf("Sending Mass Air Flow (grams/sec)\r\n");
			frame.data[3] = 20;		// Value (first byte) 7.8%
			frame.data[4] = 20;
			SendCANFrame(s, &frame);
			break;
		case PID_THROTTLE:
			printf("Sending ThrottlePosition (Percent)\r\n");
			frame.data[3] = 20;		// Value (first byte) 7.8%
			SendCANFrame(s, &frame);
			break;
		case PID_AIR_FUEL_EQUIV_RATIO:
			printf("Sending Fuel–Air commanded equivalence ratio\r\n");
			frame.data[3] = 20;		// Value (first byte)
			frame.data[4] = 20;
			SendCANFrame(s, &frame);
			break;			
		case PID_ENGINE_FUEL_RATE:
			printf("Sending Engine Fuel Rate\r\n");
			frame.data[3] = 20;		// Value (first byte)
			frame.data[4] = 20;
			SendCANFrame(s, &frame);
			break;
		default:
			printf("Unknown PID 0x%02X\r\n",PID);
			break;
	}
}

int SendCANFrame(int s, struct can_frame *frame)
{
	if (write(s, frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
		perror("Write");
		return 1;
	}
}

int Service09Response(int s, int PID)
{
	struct can_frame frame;
	frame.can_id = 0x7E8;
	frame.can_dlc = 8;
	
	printf("Service 9 PID = 0x%02X\r\n",PID);
	switch (PID) {
		case 0:
			printf("Sending Service 9 supported PIDs\r\n");
			frame.data[0] = 4;		// Number of additional bytes
			frame.data[3] = 0x40;
			frame.data[4] = 0x00;
			frame.data[5] = 0x00;
			frame.data[6] = 0x00;
			if (write(s, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
				perror("Write");
				return 1;
			}
			break;
		case 2:
			printf("Sending Vehicle Identification Number (VIN)\r\n");
			
// https://en.wikipedia.org/wiki/ISO_15765-2
//	
//			  can0  7DF   [8]  02 09 02 00 00 00 00 00
//			  can0  7E8   [8]  10 14 49 02 01 33 46 41
//			Next the requestor must send a flow control message to say: "OK to continue sending the rest of the message". The flow control message must be sent with a CAN ID that is the ID of the reply from the ECU minus 8. So the flow control here is sent with ID 0x7E0 with data 0x30 followed by 7 times 0x00.
//			  can0  7E0   [8]  30 00 00 00 00 00 00 00
//			  can0  7E8   [8]  21 44 50 34 46 4A 32 42
//			  can0  7E8   [8]  22 4D 31 31 33 39 31 33
			
			frame.data[0] = 0x10; 	// First Frame, ISO_15765-2 + high length
			frame.data[1] = 20;		// low length 
			frame.data[2] = 0x49;	// Custom Service/Mode (Same as query + 0x40) 		
			frame.data[3] = PID;	// PID
			frame.data[4] = 0x01;
			frame.data[5] = 0x00;	//VIN[0]
			frame.data[6] = 0x00;	//VIN[1]
			frame.data[7] = 0x00;	//VIN[2]
			if (write(s, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
				perror("Write");
				return 1;
			}
			break;
		default:
			printf("Unknown PID 0x%02X",PID);
			break;
	}
	
}


