
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

int main(int argc, char **argv)
{
	int s, i; 
	int nbytes;
	struct sockaddr_can addr;
	struct ifreq ifr;
	struct can_frame frame;

	printf("OBD2 Demo\r\n");

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
	} while(1);
		
	if (close(s) < 0) {
		perror("Close");
		return 1;
	}

	return 0;
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
			printf("Unknown PID 0x%02X",PID);
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


