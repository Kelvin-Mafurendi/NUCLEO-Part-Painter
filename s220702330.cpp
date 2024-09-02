//Mafurendi K, 220702330
//ECSV311 Final Project: NUCLEO Part Painting
//Deadline: 17 June 2024, 2359hrs
//___________________________________________
//Press (Ctrl + F) to change wifi credentials

//MQTT Protocol
/*MQTT topics--------------
Kelvin/PugsGo/....All topics start with the home topic prefix
Subscriptions:
NUCLEO to GUI--------------------------------
Kelvin/PugsGo/nucleo --- publish to a node status panel to show that the NUCLEO microcontroller is still connected
Kelvin/PugsGo/alert --- publish to an led panel when estop is on
Kelvin/PugsGo/msg --- publish to text log panel to send specific messages to the gui
Kelvin/PugsGo/loaded --- publish to a line graph the number of loaded pugs
Kelvin/PugsGo/painted --- pushish to a line graph the number of painted pugs
Kelvin/PugsGo/sorted ---publish to a line graph the number of sorted pugs
Kelvin/PugsGo/red -- publish to a bar graph the number of red pugs
Kelvin/PugsGo/green --- publish to a bar graph the number of green pugs
Kelvin/PugsGo/blue --- publish to a bar graph the number of blue pugs
Kelvin/PugsGo/lost -- publish to a pie chart the number of lost pugs
Kelvin/PugsGo/unpainted --- publish to a pie chart the number of unpainted pugs
Kelvin/PugsGo/bufferspeed -- publish to a guage panel the current speed of the bufferwheel
Kelvin/PugsGo/stage -- publish to a progress bar the current stage of the system in the cycle
Kelvin/PugsGo/tubes --- publish thenumber of sorting hopper tubes that are full
Kelvin/PugsGo/recipe --- publish the current recipe on the simulation

Kelvin/PugsGo/stats ---- All topics on the Statitics page/tab start with this prefix
Kelvin/PugsGo/stats/loaded -- publish the total number of pugs loaded since the process started
Kelvin/PugsGo/stats/painted --- publish the tottal numbe rof painted pugs since the start of the process
Kelvin/PugsGo/stats/sorted ---publish to a line graph the total number of sorted pugs
Kelvin/PugsGo/stats/red -- publish to a bar graph the total number of red pugs
Kelvin/PugsGo/stats/green --- publish to a bar graph the total number of green pugs
Kelvin/PugsGo/stats/blue --- publish to a bar graph the total number of blue pugs
Kelvin/PugsGo/stats/lost -- publish to a pie chart the total number of lost pugs
Kelvin/PugsGo/stats/unpainted --- publish to a pie chart the total number of unpainted pugs

GUI to NUCLEO----------------------------------------
Kelvin/PugsGo/pause --- publishes the clicking of a pause button on the home page/tab. Paylaod ==1
Kelvin/PugsGo/continue --- publishes the clicking of a continue button on the home page/tab Paylaod ==1
Kelvin/PugsGo/set/bufferspeed --- publishes the buffer wheel speed slider panel on the settings page when the value is changed 
Kelvin/PugsGo/set/crecipe --- publishes the colour recipe selected from  combo box panel on the settings page
Kelvin/PugsGo/set/tube_size --- publishes the number of pugs from a fill size text input panel on the settings page
Kelvin/PugsGo/set/tubes --- publishes require numbe r of tubes selected on the Number of tubes combo boxpanel on the settings page

-------*/

//Required Libraries
#include "mbed.h"
#include "NUCLEOPartPainting.h"
#include "ESP8266Interface.h" 
#include <MQTTClientMbedOs.h>
#include <cstdio>
#include <cstring>


//Project Peripherals
DigitalIn start(PC_1);//Black Pb : start button
DigitalIn Estop(PC_13);//BluePb : Emergency stop button
DigitalIn pause(PA_9);//SW0 : [1 - system pause, 0 - stystem continue] button
DigitalIn cont(PC_7);//
DigitalIn modebtn(PB_8);//SW7 -- To alter mode for the process
BusOut status_leds(PA_10,PB_0,PB_5,PB_4,PB_10,PA_8,PA_0,PA_4);//COntrol for the status LEDs on the SWLed  SHiled From LED0-LED7
NUCLEOPartPainting np;//UART Communication with the Simulation(Simulation Object)
ESP8266Interface wifi(PC_10, PC_11, false, NC, NC, PA_5);//ESP8266Interface object
//All physical switches ad buttons use negative logic

// Event Flag Variables
 bool ALIVE = true;//Always true
 bool BOX_LOADED;            
 bool BOX_UNLOADED;         
 bool BOX_PAINTED;           
 bool BOX_SORTED;           
 bool BOX_UNPAINTED;           
 bool BOX_LOST;               
 bool E_STOPPED;     
 bool SYS_PAUSED = false;
 bool SYS_CONT ;         
 bool MOBILE = true;            
 bool CONTINUED_MSG;      
 bool RECIPE_MSG;            
 bool TUBE_MSG;           
 bool NUCLEO_CONNECTED;      
 bool MSG;               
 bool RED;                    
 bool BLUE;                   
 bool GREEN;                 
 bool CURRENT_RECIPE;     
 bool CURRENT_SPEED = false;         
 bool SPEED_MSG;              
 bool STAGE;              
 bool Alert;                 
 bool MODE_MSG;    
 bool TEST_COMPLETE;
bool TOTAL_LOAD =false;
bool TOTAL_LOST =false;
bool TOTAL_SORT = false;
bool TOTAL_UNPAINT = false;
bool TOTAL_PAINT =false;
bool TOTAL_UNLOAD = false;    
bool TOTAL_RED =false;  
bool TOTAL_GREEN =false;   
bool TOTAL_BLUE =false; 
bool TUBE_FULL = false;   
bool DONE = false;
//_________________________________________________________________________________________________
//recipes for pug painting
enum recipe {mono_r,mono_g,mono_b,bi_rg,bi_rb,bi_gb,tri_rgb,tri_gbr,tri_brg,tri_bgr,tri_rbg};
enum mode {test,production};//process modes:
//Test Mode : Used to verify that all parts of the system are functioning as expected
//Production mode: The default operation of the system
//_________________________________________________________________________________________________

//Recipe counter variables
int cr = 0;
int cg = 0;
int cb = 0;
int crb = 0;
int crg = 0;
int cgb = 0;
int crgb = 0;
int crbg = 0;
int cbrg = 0;
int cbgr = 0;
int cgbr = 0;
int total_pugs = 3;
int tube_filled;


//Create a custom datatype called pug to represent each individual pug
typedef struct
{
    int pug_number;//basic number id for each pug from 0-8(for now)
    int colour;//colour representation of a pug [1,2,or 3] --Sort of, each pug decides its colour when it gets to the paint gun
    bool loaded;//state whether pug is loaded onto buffer(true or false)
    bool unloaded;//stet whether pug has been unloaded from buffer(true or false)
    bool painted;//state if pug has been painted(true or false)
    bool sorted;//state whether pug has been sorted into hopper(true or false)

}pug;

//Define/Create a datatype called hopper_tube to represent a sorting hopper 
//in the simulation with the following properties:
typedef struct
{
    bool empty;//true when the hopper tube is empty
    bool full;//true when the hopper tube is full
    int size;//the number of pugs required to fill the hopper tube


} hopper_tube;


//===========================================Initialize Global variables=========================================================
int colour =0;//variable to store the colour to be painted on a pug
int tubes_required = 3;//the number fo tubes to be fuillled in a session
int speed = 10;//default set at 5
int load_count,unload_count,paint_count,unpainted,lost = 0;///Pug Counter variables for different processes
int r,g,b;//Pug counters by colour
int total_r,total_g,total_b = 0;
int sorted = 0;//Initialize the total number of pugs that have gone through the entire process
int stage = 0; //Simulate the current stage of the process[1-5]
int t;//variable to store the number of test cycles[1-3]
int state = 0;//state variable for production mode
int test_state = 0;//state variable for test mode
int last_state;//variable to help rememeber the last state before pausing
int lift_states = 0;
bool lift_ready = false;
int fill_size = 3;
int pug_counter = -1;
int total_lost_pugs, total_painted,total_loaded,total_unpaint,total_sorted, total_unload = 0;
const int pause_state = 1000000;//special pause state
const int estop_state = 5000000;//special estop state
bool paused = false;//bool variable to represent if system is paused or not
bool estop = false;//bool variable to represent if system is in emergency stop condition
Timer s;
float initialAngle = np.BufferWheelCounter();
float targetAngle = initialAngle + float(5); // Move 45 degrees
char messages[255];//varible to store test messages to send to the gui
//custom types and enums declarations
hopper_tube tubes[3];
recipe recipes;//declare a recipe variable
recipe rec = tri_rgb;
mode mode;
pug pugs[30];//create an array of pugs

//=================================================MQTT Variables==========================================
int mqtt_bufferSpeed = 0;//stores set bufferwheel speed from GUI
//int mqtt_mode = 0;//stores set mode from GUI
char mqtt_recipe[10];//stores set painting recipe/sequence from GUI
int mqtt_tubes = 1;////stores set number of hopper tubes to be filled from GUI
int mqtt_pause = 0;//stores value when pause is pressed from GUI ---working
int mqtt_continue = 0;//stores value when continue is pressed from GUI----working
//int mqtt_mobile = 0;
int mqtt_size = 0;




//==============================================Supporting/Callback funtion prototypes=====================================================

void init_pug(recipe choice);//function prototype for pug initialization
void init_tube(int fill);//hopper tube initialization
void buttonhandler();//function to handdle user buttons
void run_pause();
void run_continue();
void handletubes();
void fillMsgArrived(MQTT::MessageData& md);
void tubesMsgArrived(MQTT::MessageData& md);
void pauseMsgArrived(MQTT::MessageData& md);
void continueMsgArrived(MQTT::MessageData& md);
//void mobileMsgArrived(MQTT::MessageData& md);
//void modeMsgArrived(MQTT::MessageData& md);
void speedMsgArrived(MQTT::MessageData& md);
void recipeMsgArrived(MQTT::MessageData& md);
int publish(MQTTClient *pclient, const char *topic, const char *msg, int qos, bool retain);
void mqtt_wifi();//callback function for comms



//===========================================Thread API and EventFlag API Definitions======================================================
Thread commsThread;//(osPriorityHigh,3*1024);///check how this affects operation

//================================================Main Thread=======================================================
int main()
{
    //start comms thread
    commsThread.start(callback(mqtt_wifi));
    init_tube(fill_size);//initialize sorting hoppers
    while(1)
    { 
        

        buttonhandler();
        handletubes();
        

        // Update speed if a new value is received
        if (CURRENT_SPEED) 
        {
            speed = mqtt_bufferSpeed;
            CURRENT_SPEED = false; // Reset the flag after updating the speed
            SPEED_MSG = true;
        }

        
        switch(mode)
        { 
            case test:
            { 
                switch(test_state)
                {
                case 0://HomeState
                {
                        //Reset EVerything
                        status_leds = 0b01010101;
                        sprintf(messages, "Test Mode On\n");
                        
                        np.printf(messages);
                        load_count = 0;
                        unload_count = 0;
                        paint_count = 0;
                        r = 0;
                        g = 0;
                        b = 0;
                        t = 0;
                        colour = 1;
                        
                        if (start == 0)
                        {
                            ThisThread::sleep_for(100ms);//allow for contact bounce
                            if(start == 0)
                            {
                                MSG = true;
                                test_state = 10;
                            }
                        }
                    }//end case
                    break;
                    case 10:
                    {
                        status_leds = 0b00000001;
                        
                        if(np.HopperCylinderBackSensor() == 1 && t < 3)
                        {
                            test_state = 11;
                        }
                        else if(t > 2)
                        {
                            np.printf("Test Complete!\n");
                            test_state = 0;
                        }
                    }//end case
                    break;
                    case 11:
                    {
                        np.HopperCylinder(1);
                        if(np.HopperCylinderFrontSensor() == 1 && np.HopperProxy() == 1)
                        {
                            test_state = 12;
                        }
                    }//end case
                    break;
                    case 12:
                    {
                        ThisThread::sleep_for(100ms);//Allow pug to stabilize
                        test_state = 13;
                    }//end case
                    break;
                    case 13:
                    {
                        np.HopperCylinder(0);
                        if(np.HopperCylinderBackSensor() == 1)
                        {
                            test_state = 14;
                        }
                    }//end case
                    break;
                    case 14:
                    {
                        ///Do nothing
                        if(np.LoadProxy() == 1)
                        {
                            test_state = 20;
                        }
                    }//end case
                    break;
                    case 20:
                    {
                        status_leds = 0b00000010;
                        ThisThread::sleep_for(100ms);//Allow pug to stabilize
                        if(np.LoadCylinderBackSensor() == 1)
                        {
                            test_state = 21;
                        }
                    }//end case
                    break;
                    case 21:
                    {
                        np.LoadCylinder(1);
                        if (np.LoadCylinderFrontSensor() == 1)
                        {
                            test_state = 22;
                        }
                    }//end case
                    break;
                    case 22:
                    {
                        np.LoadCylinder(0);
                        if(np.LoadCylinderBackSensor() == 1)
                        {  
                            test_state = 23;
                        }
                    }//end case
                    break;
                    case 23:
                    {
                        load_count ++;
                        np.printf("Loaded Pugs: %d\n",load_count);
                        test_state = 30;
                    }//end case
                    break;
                    case 30:
                    {
                        
                        do
                        {
                            np.BufferWheel(speed);
                        }while(np.BufferWheelCounter() < float(89.75) || np.BufferWheelCounter() >  float(90.25));
                        test_state = 31;
                    }
                    break;
                    case 31:
                    {
                        np.BufferWheel(0);
                        np.PaintGun(colour);
                        ThisThread::sleep_for(4s);
                        np.PaintGun(0);
                        paint_count ++;
                        colour ++;
                        np.printf("Painted Pugs: %d\n",paint_count);
                        test_state = 32;

                    }//end case
                    break;
                    case 32:
                    {
                        do
                        {
                            np.BufferWheel(speed);
                        }while(np.BufferWheelCounter() < float(329.75) || np.BufferWheelCounter() >  float(330.25));
                        test_state = 33;

                    }//end case
                    break;
                    case 33:
                    {
                        np.BufferWheel(0);
                        test_state = 35;
                    }
                    break;
                    case 34:
                    {
                        do
                        {
                            np.BufferWheel(-speed);
                        }while(np.BufferWheelCounter() < float(14.75) || np.BufferWheelCounter() >  float(15.25));
                        test_state = 35;
                        
                    }
                    break;
                    case 35:
                    {
                        np.BufferWheel(0);
                        np.Servo(15);
                        if(np.UnloadCylinderBackSensor() == 1)
                        {
                            test_state = 36;
                        }
                    }
                    break;
                    case 36:
                    {
                        np.UnloadCylinder(1);
                        if(np.UnloadCylinderFrontSensor() == 1)
                        {
                            test_state = 37;
                        }
                    }
                    break;
                    case 37:
                    {
                        np.UnloadCylinder(0);
                        if(np.UnloadCylinderBackSensor() == 1)
                        test_state =38;
                    }
                    break;
                    case 38:
                    {
                        if(np.LiftProxy1() == 1)
                        {
                            test_state = 39;
                        }
                    }
                    break;
                    case 39:
                    {
                        np.Servo(74);
                        if(np.LiftProxy2() == 1)
                        {
                            test_state = 40;
                        }
                    }
                    break;
                    case 40:
                    {
                        if(np.LiftProxy2() == 0)
                            {
                                test_state = 41;
                            }
                    }
                    break;
                    case 41:
                    {
                        
                        do
                        {
                            np.BufferWheel(speed);
                        }while(np.BufferWheelCounter() < float(358.75) || np.BufferWheelCounter() >  float(0.25));
                        test_state = 42;
                        
                    }
                    break;
                    case 42:
                    {
                        np.BufferWheel(0);
                        sorted ++;
                        t ++;
                        test_state = 10;
                    }
                    break;
                    case 5000000:
                    {
                        np.BufferWheel(0);
                        np.printf("Emergency Stop!\n");
                        //Send message to mqtt(emergency alert)
                        np.Guide(0);
                        np.LoadCylinder(0);
                        np.UnloadCylinder(0);
                        np.HopperCylinder(0);
                        np.Servo(0);
                        while(estop)
                        {
                            //Alert LEDs
                            status_leds = 0b00000000;
                            ThisThread::sleep_for(50ms);
                            status_leds = 0b00000001;
                            ThisThread::sleep_for(50ms);
                            status_leds = 0b00000011;
                            ThisThread::sleep_for(50ms);
                            status_leds = 0b00000111;
                            ThisThread::sleep_for(50ms);
                            status_leds = 0b00001111;
                            ThisThread::sleep_for(50ms);
                            status_leds = 0b00011111;
                            ThisThread::sleep_for(50ms);
                            status_leds = 0b00111111;
                            ThisThread::sleep_for(50ms);
                            status_leds = 0b01111111;
                            ThisThread::sleep_for(50ms);
                            status_leds = 0b11111111;
                        }
                    }
                    break;
                    case 1000000://special Pause state
                    {
                        np.BufferWheel(0);
                        //Send message to mqtt(emergency alert)
                        np.Guide(0);
                        np.LoadCylinder(0);
                        np.UnloadCylinder(0);
                        np.HopperCylinder(0);
                        //SYS_PAUSED = true;
                        while(paused)
                        {
                            buttonhandler();
                            //Pause Display
                            status_leds = 0b00000001;
                            ThisThread::sleep_for(50ms);
                            status_leds = 0b00000010;
                            ThisThread::sleep_for(50ms);
                            status_leds = 0b00000100;
                            ThisThread::sleep_for(50ms);
                            status_leds = 0b00001000;
                            ThisThread::sleep_for(50ms);
                            status_leds = 0b00010000;
                            ThisThread::sleep_for(50ms);
                            status_leds = 0b00100000;
                            ThisThread::sleep_for(50ms);
                            status_leds = 0b01000000;
                            ThisThread::sleep_for(50ms);
                            status_leds = 0b10000000;
                            ThisThread::sleep_for(50ms);
                        }
                    }
                    break;
                    default:
                    {
                        test_state = 10;
                    }

                }
            break;
            case production:
            {
                switch(state)//Main Process State machine
                {
                    case 0://HomeState
                    {
                        //Reset EVerything
                        status_leds = 0b00000000;
                        sorted = 0;
                        unpainted = 0;
                        paint_count = 0;
                        load_count = 0;
                        unload_count = 0;
                        lost = 0;
                        total_lost_pugs =0;
                        total_loaded = 0;
                        total_painted =0;
                        total_sorted = 0;
                        total_unpaint = 0;
                        total_r = 0;
                        total_b = 0;
                        total_g = 0;
                        stage  = 0;
                        STAGE = true;
                        state = 5;
        
                    }//end case
                    break;
                    case 5:
                    {
                        status_leds = 0b11111111;
                        sprintf(messages, "Production mode engaded\n");
                        np.printf(messages);
                        tube_filled = -1;
                        init_pug(rec);
                        
                        
                        
                        
                        
                        if (start == 0)
                        {

                            ThisThread::sleep_for(100ms);
                            if(start == 0)
                            {
                                MSG = true;
                                RECIPE_MSG = true;
                                TUBE_FULL = true;
                                state = 1110;
                            }
                        }
                    }
                    break;
                    case 1110:
                    {
                        status_leds = 0b00000001;
                        /*total_loaded = total_loaded + load_count;
                        total_painted = total_painted + paint_count;
                        total_unpaint = total_unpaint + unpainted;
                        total_sorted = total_sorted + sorted;
                        total_lost_pugs = total_lost_pugs + lost;
                        total_b = total_b + b;
                        total_g = total_g + g;
                        total_b = total_b + b;*/
                        
                        
                      
                        if (ALIVE == true)
                        {
                            TOTAL_BLUE = true;
                            TOTAL_GREEN = true;
                            TOTAL_RED = true;
                            TOTAL_LOAD = true;
                            TOTAL_SORT = true;
                            TOTAL_LOST = true;
                            TOTAL_PAINT = true;
                            TOTAL_UNPAINT = true;
                           
                            
                            state =1120;    

                        }
                        
                        
                        
                        
                        
                    }//end case

                    break;
                    case 1120:
                    {
                        stage = 1;
                        load_count = 0;
                        unload_count = 0;
                        paint_count = 0;
                        lost = 0;
                        unpainted = 0;

                        r = 0;
                        g = 0;
                        b = 0;
                        
                        
                        
                        if(ALIVE == true)
                        {
                            BOX_LOADED =true;
                            BOX_SORTED = true;
                            BOX_PAINTED = true;
                            BOX_UNPAINTED =true;
                            BOX_LOST =true;
                            RED = true;
                            GREEN = true;
                            BLUE = true;
                            STAGE = true;///a new stage has been reached
                            state = 10;
                        }
                    }
                    break;
                    case 10:
                    {
                        if(np.HopperCylinderBackSensor() == 1 && DONE == false)
                        {
                            s.reset();
                            s.start();
                            state = 11;
                        }

                    }
                    case 11:
                    {
                        
                        np.HopperCylinder(1);
                        if(np.HopperCylinderFrontSensor() == 1 && np.HopperProxy() == 1)
                        {
                            s.reset();
                            state = 12;
                        }

                        if(s.elapsed_time() > 5s)//faulty cylinder
                        {
                            s.reset();
                            sprintf(messages, "Hopper cylinder error!\n");
                            np.printf(messages);
                            if(np.UnloadCylinderFrontSensor() == 1)
                            {
                                MSG = true;
                                state = 43;
                            }
                            MSG = true;
                            ThisThread::sleep_for(5ms);
                            MSG = false; //so that MSG does not stay true and keep sennding messages
                            
                        }
                    }//end case
                    break;
                    break;
                    case 12:
                    {
                        ThisThread::sleep_for(500ms);//Allow pug to stabilize for half a second 500ms
                        state = 13;
                    }//end case
                    break;
                    case 13:
                    {
                        s.reset();
                        s.start();
                        np.HopperCylinder(0);
                        if(np.HopperCylinderBackSensor() == 1)
                        {
                            state = 14;
                        }
                    }//end case
                    break;
                    case 14:
                    {
                        ///Do nothing
                        if(np.LoadProxy() == 0 && np.HopperCylinderBackSensor() == 1 && s.elapsed_time() > 30s)
                        {
                            //puglost
                            s.reset();
                            sprintf(messages, "Pug retrival error,\nSystem might need a reset...\n");
                            np.printf(messages);
                            MSG = true;
                            state = 10;//go back to10 /// or alert operator to reset
                        }
                        if(np.LoadProxy() == 1)
                        {
                            state = 20;
                        }
                    }//end case
                    break;
                    case 20:
                    {
                        s.reset();
                        status_leds = 0b00000010;
                        stage = 2;
                        STAGE = true;
                        ThisThread::sleep_for(100ms);//Allow pug to stabilize
                        if(np.LoadCylinderBackSensor() == 1 && DONE == false)
                        {
                            s.reset();
                            s.start();
                            state = 21;
                        }
                    }//end case
                    break;
                    case 21:
                    {
                        
                        np.LoadCylinder(1);
                        if (np.LoadCylinderFrontSensor() == 1)
                        {
                            s.reset();
                            state = 22;
                        }
                        if(s.elapsed_time() > 5s)//faulty cylinder
                        {
                            s.reset();
                            sprintf(messages, "Load cylinder error!\n");
                            np.printf(messages);
                            if(np.LoadCylinderFrontSensor() == 1)
                            {
                                MSG = true;
                                state = 43;
                            }
                            MSG = true;
                            ThisThread::sleep_for(5ms);
                            MSG = false; //so that MSG does not stay true and keep sennding messages
                        }
                    }//end case
                    break;
                    case 22:
                    {
                       
                        np.LoadCylinder(0);
                        if(np.LoadCylinderBackSensor() == 1)
                        {  
                            state = 23;
                        }
                    }//end case
                    break;
                    case 23:
                    {
                        load_count ++;
                        total_loaded ++;
                        BOX_LOADED = true;
                        TOTAL_LOAD =true;
                        np.printf("Loaded Pugs: %d\n",load_count);
                        
                        
                        state = 30;
                    }//end case
                    break;
                    case 30:
                    {
                        status_leds = 0b00000100;
                        stage =3;
                        STAGE = true;
                        if(load_count == 1)
                        {
                            state = 31;
                        }
                        else if(load_count == 2)
                        {
                            state = 32;
                        }
                        else if(load_count == 3)
                        {
                            state = 33;
                        }
                        else if(load_count == 4)
                        {
                            state = 34;
                        }
                        else if(load_count == 5)
                        {
                            state = 35;
                        }
                        else if(load_count == 6)
                        {
                            state = 36;
                        }
                        else if(load_count == 7)
                        {
                            state = 37;
                        }
                        else if(load_count == 8)
                        {
                            state = 38;
                        }
                    }//end case
                    break;
                    case 31:
                    {
                        
                        do
                        {
                            np.BufferWheel(speed);
                            ThisThread::sleep_for(10ms);
                        }while(np.BufferWheelCounter() < float(44.75) || np.BufferWheelCounter() >  float(45.25));
                        state = 2000;
                    }//end case
                    break;
                    case 32:
                    {
                        //colour = pugs[0].colour;
                        do
                        {
                            np.BufferWheel(speed);
                        }while(np.BufferWheelCounter() < float(89.5) || np.BufferWheelCounter() >  float(90));
                        state = 39;
                    }//end case
                    break;
                    case 33:
                    {
                        //colour = pugs[1].colour;
                        do
                        {
                            np.BufferWheel(speed);
                        }while(np.BufferWheelCounter() < float(134.75) || np.BufferWheelCounter() >  float(135.25));
                        state = 39;
                    }//end case
                    break;
                    case 34:
                    {
                        //colour = pugs[2].colour;
                        do
                        {
                            np.BufferWheel(speed);
                        }while(np.BufferWheelCounter() < float(179.75) || np.BufferWheelCounter() >  float(180.25));
                        state = 39;
                    }//end case
                    break;
                    case 35:
                    {
                        //colour = pugs[3].colour;
                        do
                        {
                            np.BufferWheel(speed);
                        }while(np.BufferWheelCounter() < float(224.75) || np.BufferWheelCounter() >  float(225.25));
                        state = 39;
                    }//end case
                    break;
                    case 36:
                    {
                        //colour = pugs[4].colour;
                        do
                        {
                            np.BufferWheel(speed);
                        }while(np.BufferWheelCounter() < float(269.75) || np.BufferWheelCounter() >  float(270.25));
                        state = 39;
                    }//end case
                    break;
                    case 37:
                    {
                        //colour = pugs[5].colour;
                        do
                        {
                            np.BufferWheel(speed);
                        }while(np.BufferWheelCounter() < float(314.75) || np.BufferWheelCounter() >  float(315.25));
                        state = 39;
                    }//end case
                    break;
                    case 38:
                    {
                        //colour = pugs[6].colour;
                        do
                        {
                            np.BufferWheel(speed);
                            ThisThread::sleep_for(10ms);
                        } while (!(np.BufferWheelCounter() >= float(359) || np.BufferWheelCounter() < float(0.75)));
                        state = 39;
                    }//end case
                    break;
                    case 3000:
                    {
                        //colour = pugs[7].colour;
                        do
                        {
                            np.BufferWheel(speed);
                        }while(np.BufferWheelCounter() < float(43.25) || np.BufferWheelCounter() >  float(44));
                        state = 39;
                    }//end case
                    break;
                    case 2000:
                    {
                        np.BufferWheel(0);
                        state = 10;
                    }//end case
                    break;
                    case 39:
                    {
                        np.BufferWheel(0);
                        state = 1000;

                    }   //end case
                    
                    break;
                    
                    case 1000://Painting State
                    {
                        status_leds = 0b00001000;
                        stage =4;
                        STAGE =  true;
                        pug_counter ++;
                        colour = pugs[pug_counter].colour;
                    
                        np.PaintGun(colour);
                        ThisThread::sleep_for(4s);
                        np.PaintGun(0);
                        paint_count ++;
                        total_painted ++;
                        np.printf("Painted Pugs: %d\n",paint_count);
                        //np.printf("Colour: %d", colour);
                        BOX_PAINTED = true;
                        TOTAL_PAINT =true;

                        
                        
                        if(paint_count <= 6 )
                        {
                            state = 10;
                        }
                        else if(paint_count == 7)
                        {
                            state = 3000;
                        }
                        else if(paint_count == 8)
                        {
                            state = 40;
                        }
                        
                    }//end case
                    break;
                    case 40:
                    {
                        status_leds = 0b00010000;//Unloading
                        stage =5;
                        STAGE = true;
                        do
                        {
                            
                            np.BufferWheel(-speed);
                            //ThisThread::sleep_for(10ms);
                        }while(np.BufferWheelCounter() < float(16.25) || np.BufferWheelCounter() >  float(17.25));
                        s.reset();
                        state = 41;

                    }//end case
                    break;
                    case 41:
                    {
                        np.BufferWheel(0);

                        if(np.ColourSensorOut1() == 1)
                        {
                            r ++;
                            total_r ++;
                            RED = true;
                            TOTAL_RED =true;
                            
                            
                        }
                        else if(np.ColourSensorOut2() == 1)
                        {
                            g ++;
                            total_g ++;
                            GREEN = true;
                            TOTAL_GREEN = true;
                            
                        }
                        else if(np.ColourSensorOut3() == 1)
                        {
                            b ++;
                            total_b ++;
                            BLUE = true;
                            TOTAL_BLUE = true;
                            
                            
                        }
                        else 
                        {
                            unpainted ++;
                            total_unpaint ++;
                            paint_count --;
                            total_painted --;
                            BOX_PAINTED = true;
                            TOTAL_PAINT =true;
                            BOX_UNPAINTED =  true;
                            TOTAL_UNPAINT = true;
                            
                        }
                        np.Servo(15);
                        lift_ready = true;
                        if(np.UnloadCylinderBackSensor() == 1  && lift_ready == true)
                        {
                            s.reset();
                            s.start();
                            state = 42;
                        }
                    }//end case
                    break;
                    case 42:
                    {
                        status_leds = 0b00100000;
                        stage = 6;
                        STAGE = true;
                        
                        np.UnloadCylinder(1);
                        ThisThread::sleep_for(2s);//Allow thourough exit of pug from the wheel
                        
                        
                        if(np.UnloadCylinderFrontSensor() == 1)
                        {
                            s.reset();
                            state = 43;
                        }

                        if(s.elapsed_time() > 7s)//faulty cylinder
                        {
                            s.reset();
                            sprintf(messages, "Unload cylinder error!\n");
                            np.printf(messages);
                            if(np.UnloadCylinderFrontSensor() == 1)
                            {
                                MSG = true;
                                state = 43;
                            }
                            MSG = true;
                            ThisThread::sleep_for(5ms);
                            MSG = false; //so that MSG does not stay true and keep sennding messages
                        }
                    }//end case
                    break;
                    case 43:
                    {
                        
                        np.UnloadCylinder(0);
                        if(np.UnloadCylinderBackSensor() ==1)
                        {
                            state  = 4000;
                        }
                    }//end case
                    break;
                    case 4000:
                    {
                        
                        unload_count ++;
                        total_unload ++;
                        BOX_UNLOADED = true;
                        np.printf("Unloaded Pugs: %d\n",unload_count);
                        BOX_SORTED = false;//be sure sorted is false
                        state = 44;
                    }
                    break;
                    case 44:
                    {
                        //start timer or check error()
                        // Start the timer if it hasn't started yet
                        
                        s.start();
                        
                        if(np.LiftProxy1() == 1)
                        {
                            s.reset();
                            state = 45;
                        }
                        if((s.elapsed_time() > 30s))
                        {
                            s.reset();
                            state = 460;
                        }   
                        
                    }//end case
                    break;
                    case 460:
                    {
                        //s.reset();
                        lost  ++;
                        total_lost_pugs ++;
                        sprintf(messages, "One pug lost!\n");
                        np.printf("Lost pugs: %d\n", lost);
                        MSG = true;
                        BOX_LOST = true;
                        TOTAL_LOST = true;
                        state = 46;
                    }
                    break;
                    case 45:
                    {
                        //s.reset();
                        lift_ready = false;
                        np.Servo(74);
                        if(np.LiftProxy2() == 1)
                        {
                            state = 450;
                        }
                    }//end case
                    break;
                    case 450:
                    {
                        if(np.LiftProxy2() == 0)
                            {
                                state = 451;
                            }
                    }
                    break;
                    case 451:
                    {

                        sorted ++;
                        total_sorted ++;
                        np.printf("Current Pugs sorted: %d\nTotal sorted %d\n",sorted,total_sorted);
                        BOX_SORTED =true;
                        TOTAL_SORT = true;
                        state = 46;
                    }
                    break;

                    case 46:
                    {
                        //s.reset();
                        ThisThread::sleep_for(1s);//allow pug to be well placed 
                        BOX_SORTED = false;
                        np.Servo(15);
                        if(unload_count == 1)
                        {
                            state = 51;
                        }

                        else if(unload_count == 2)
                        {
                            state = 52;
                        }
                        else if(unload_count == 3)
                        {
                            state = 53;
                        }
                        else if(unload_count == 4)
                        {
                            state = 54;
                        }
                        else if(unload_count == 5)
                        {
                            state = 55;
                        }
                        else if(unload_count == 6)
                        {
                            state = 56;
                        }
                        else if(unload_count == 7)
                        {
                            state = 57;
                        }
                        else if(unload_count == 8)
                        {
                            state = 58;
                        }
                    }//end case 
                    break;
                    case 51:
                    {
                        do
                        {
                            
                            np.BufferWheel(speed);
                        }while(np.BufferWheelCounter() < float(57) || np.BufferWheelCounter() >  float(58));
                        state = 41;
                    }//end case
                    break;
                    case 52:
                    {
                        do
                        {
                            
                            np.BufferWheel(speed);
                        }while(np.BufferWheelCounter() < float(103) || np.BufferWheelCounter() >  float(103.75));
                        state = 41;
                    }//end case
                    break;
                    case 53:
                    {
                        do
                        {
                            
                            np.BufferWheel(speed);
                        }while(np.BufferWheelCounter() < float(147.25) || np.BufferWheelCounter() >  float(148));
                        state = 41;
                    }//end case
                    break;
                    case 54:
                    {
                        do
                        {
                            
                            np.BufferWheel(speed);
                        }while(np.BufferWheelCounter() < float(192.75) || np.BufferWheelCounter() >  float(193.25));
                        state = 41;
                    }//end case
                    break;
                    case 55:
                    {
                        do
                        {
                            
                            np.BufferWheel(speed);
                        }while(np.BufferWheelCounter() < float(238) || np.BufferWheelCounter() >  float(239.25));
                        state = 41;
                    }//end case
                    break;
                    case 56:
                    {
                        do
                        {
                            
                            np.BufferWheel(speed);
                        }while(np.BufferWheelCounter() < float(282) || np.BufferWheelCounter() >  float(283.25));
                        state = 41;
                    }//end case
                    break;
                    case 57:
                    {
                        do
                        {
                            
                            np.BufferWheel(speed);
                        }while(np.BufferWheelCounter() < float(327) || np.BufferWheelCounter() >  float(329.25));
                        state = 41;
                    }//end case
                    break;
                    case 58:
                    {
                        do
                        {
                            np.BufferWheel(speed);
                            ThisThread::sleep_for(10ms);
                        } while (!(np.BufferWheelCounter() >= float(359) || np.BufferWheelCounter() <= float(0.25)));
                        
                        np.BufferWheel(0);
                        sprintf(messages, "1 Cycle Complete!\n");
                        np.printf(messages);
                        MSG = true;
                        state = 1110;
                    }
                    break;
                    case 5000000:
                    {
                        np.BufferWheel(0);
                        E_STOPPED =true;
                        np.printf("Emergency Stop!\n");
                        //Send message to mqtt(emergency alert)
                        np.Guide(0);
                        np.LoadCylinder(0);
                        np.UnloadCylinder(0);
                        np.HopperCylinder(0);
                        np.Servo(0);
                        while(estop)
                        {
                            //Alert LEDs
                            status_leds = 0b00000000;
                            ThisThread::sleep_for(50ms);
                            status_leds = 0b00000001;
                            ThisThread::sleep_for(50ms);
                            status_leds = 0b00000011;
                            ThisThread::sleep_for(50ms);
                            status_leds = 0b00000111;
                            ThisThread::sleep_for(50ms);
                            status_leds = 0b00001111;
                            ThisThread::sleep_for(50ms);
                            status_leds = 0b00011111;
                            ThisThread::sleep_for(50ms);
                            status_leds = 0b00111111;
                            ThisThread::sleep_for(50ms);
                            status_leds = 0b01111111;
                            ThisThread::sleep_for(50ms);
                            status_leds = 0b11111111;
                        }
                    }
                    break;
                    case 1000000://special Pause state
                    {
                        np.BufferWheel(0);
                        //Send message to mqtt(emergency alert)
                        np.Guide(0);
                        np.LoadCylinder(0);
                        np.UnloadCylinder(0);
                        np.HopperCylinder(0);
                        while(paused)
                        {
                            buttonhandler();
                            //Pause Display
                            status_leds = 0b00000001;
                            ThisThread::sleep_for(50ms);
                            status_leds = 0b00000010;
                            ThisThread::sleep_for(50ms);
                            status_leds = 0b00000100;
                            ThisThread::sleep_for(50ms);
                            status_leds = 0b00001000;
                            ThisThread::sleep_for(50ms);
                            status_leds = 0b00010000;
                            ThisThread::sleep_for(50ms);
                            status_leds = 0b00100000;
                            ThisThread::sleep_for(50ms);
                            status_leds = 0b01000000;
                            ThisThread::sleep_for(50ms);
                            status_leds = 0b10000000;
                            ThisThread::sleep_for(50ms);
                        }
                    }
                    break;
                    default:
                    {
                        state = 0;
                    }
                }
            }
            break;
            default:
            {
                mode = test;
            }
            
        }//end switch

        
    }//end super loop
}//end main
}

//Custom function definitions
//A function to initialize pug values for a session
void init_pug(recipe choice)//takes in a recipe as input
{
    switch(choice)
    {
        case mono_r:
        {
            for(int i = 0 ; i < total_pugs ; i ++)
            {
                pugs[i].pug_number = i+1;
                pugs[i].loaded = false;
                pugs[i].unloaded = false;
                pugs[i].painted = false;
                pugs[i].sorted = false;
                pugs[i].colour = 1;
                //np.printf("pug number: %d",pug);
            }

        }//end case
        break;
        case mono_g:
        {
            for(int i = 0;i < total_pugs ;i++)
            {
                pugs[i].pug_number = i+1;
                pugs[i].loaded = false;
                pugs[i].unloaded = false;
                pugs[i].painted = false;
                pugs[i].sorted = false;
                pugs[i].colour = 2;
            }
        }//end case
        break;
        case mono_b:
        {
            for(int i = 0;i < total_pugs ;i++)
            {
                pugs[i].pug_number = i+1;
                pugs[i].loaded = false;
                pugs[i].unloaded = false;
                pugs[i].painted = false;
                pugs[i].sorted = false;
                pugs[i].colour = 3;
            }
        }//end case
        break;
        case bi_gb:
        {
            for(int i = 0;i < total_pugs ;i++)
            {
                pugs[i].pug_number = i+1;
                pugs[i].loaded = false;
                pugs[i].unloaded = false;
                pugs[i].painted = false;
                pugs[i].sorted = false;
                
            }
            for(int i = 1;i<  total_pugs ;i+=2)
            {
                pugs[i].colour = 2;
                
            }

            for(int i = 0; i < total_pugs; i += 2)
            {
                pugs[i].colour = 3;
            }
        }//end case
        break;
        case bi_rb:
        {
            for(int i = 0;i < total_pugs ;i++)
            {
                pugs[i].pug_number = i+1;
                pugs[i].loaded = false;
                pugs[i].unloaded = false;
                pugs[i].painted = false;
                pugs[i].sorted = false;
                
            }
            for(int i = 1;i<  total_pugs ;i+=2)
            {
                pugs[i].colour = 1;
                
            }

            for(int i = 0; i < total_pugs; i += 2)
            {
                pugs[i].colour = 3;
            }
        }//end case
        break;
        case bi_rg:
        {
            for(int i = 0;i < total_pugs ;i++)
            {
                pugs[i].pug_number = i+1;
                pugs[i].loaded = false;
                pugs[i].unloaded = false;
                pugs[i].painted = false;
                pugs[i].sorted = false;
                
            }
            for(int i = 1;i <  total_pugs ;i += 2)
            {
                pugs[i].colour = 1;
                
            }

            for(int i = 0; i < total_pugs; i += 2)
            {
                pugs[i].colour = 2;
            }
        }//end case
        break;
        case tri_rgb:
        {
            for(int i = 0;i < total_pugs ;i++)
            {
                pugs[i].pug_number = i+1;
                pugs[i].loaded = false;
                pugs[i].unloaded = false;
                pugs[i].painted = false;
                pugs[i].sorted = false;
                
            }
            for(int i = 1;i <  total_pugs ;i+=3)
            {
                pugs[i].colour = 1;
                
            }
            pugs[0].colour = 2;
            pugs[2].colour = 2;
            pugs[5].colour =2;
            for(int i = 11; i < total_pugs; i += 3)
            {
                pugs[i].colour = 2;
            }
             for(int i = 3; i < total_pugs ; i += 3)
             {
                 pugs[i].colour = 3;
             }
        }//end case
        break;
        break;
        case tri_gbr:
        {
            for(int i = 0;i < total_pugs ;i++)
            {
                pugs[i].pug_number = i+1;
                pugs[i].loaded = false;
                pugs[i].unloaded = false;
                pugs[i].painted = false;
                pugs[i].sorted = false;
                
            }
            for(int i = 1;i <  total_pugs ;i+=3)
            {
                pugs[i].colour = 2;
                
            }
            pugs[0].colour = 3;
            pugs[2].colour = 3;
            pugs[5].colour =3;
            for(int i = 11; i < total_pugs; i += 3)
            {
                pugs[i].colour = 3;
            }
             for(int i = 3; i < total_pugs ; i += 3)
             {
                 pugs[i].colour = 2;
             }
        }//end case
        break;
        break;
        case tri_brg:
        {
            for(int i = 0;i < total_pugs ;i++)
            {
                pugs[i].pug_number = i+1;
                pugs[i].loaded = false;
                pugs[i].unloaded = false;
                pugs[i].painted = false;
                pugs[i].sorted = false;
                
            }
            for(int i = 1;i <  total_pugs ;i+=3)
            {
                pugs[i].colour = 3;
                
            }
            pugs[0].colour = 1;
            pugs[2].colour = 1;
            pugs[5].colour =1;
            for(int i = 11; i < total_pugs; i += 3)
            {
                pugs[i].colour = 1;
            }
             for(int i = 3; i < total_pugs ; i += 3)
             {
                 pugs[i].colour = 2;
             }
        }//end case
        break;
        break;
        case tri_bgr:
        {
            for(int i = 0;i < total_pugs ;i++)
            {
                pugs[i].pug_number = i+1;
                pugs[i].loaded = false;
                pugs[i].unloaded = false;
                pugs[i].painted = false;
                pugs[i].sorted = false;
                
            }
            for(int i = 1;i <  total_pugs ;i+=3)
            {
                pugs[i].colour = 3;
                
            }
            pugs[0].colour = 2;
            pugs[2].colour = 2;
            pugs[5].colour =2;
            for(int i = 11; i < total_pugs; i += 3)
            {
                pugs[i].colour = 2;
            }
             for(int i = 3; i < total_pugs ; i += 3)
             {
                 pugs[i].colour = 1;
             }
        }//end case
        break;
        break;
        case tri_rbg:
        {
            for(int i = 0;i < total_pugs ;i++)
            {
                pugs[i].pug_number = i+1;
                pugs[i].loaded = false;
                pugs[i].unloaded = false;
                pugs[i].painted = false;
                pugs[i].sorted = false;
                
            }
            for(int i = 1;i <  total_pugs ;i+=3)
            {
                pugs[i].colour = 1;
                
            }
            pugs[0].colour = 3;
            pugs[2].colour = 3;
            pugs[5].colour =3;
            for(int i = 11; i < total_pugs; i += 3)
            {
                pugs[i].colour = 3;
            }
             for(int i = 3; i < total_pugs ; i += 3)
             {
                 pugs[i].colour = 2;
             }
        }//end case
        break;
        default:
        {
            recipes = mono_r;
        }
    }//end switch
}//end func
void handletubes() {
    
    
        if (sorted == tubes[0].size && tubes[0].full == false) {

        sprintf(messages,"Hopper 1 is full!\n");
        np.printf(messages);
        MSG = true;
        TUBE_FULL = true;
        tubes[0].full = true;
        sorted = 0;
        if(tubes_required > 1)
        {
            float initialPos = np.GuideCounter();
            ThisThread::sleep_for(4s);//allow pug to bre funnel into hopper and the initial position to be deduced
            np.Guide(40);
            while (np.GuideCounter() < initialPos + float(41.45)) {//40.5 work well, also .75 i guess
                ThisThread::sleep_for(10ms); // Small sleep interval for better control
            }
            np.Guide(0);
        }
        else  
        {
            sprintf(messages,"Task Complete!\n");
            np.printf(messages);
            MSG = true;
            DONE = true;
            ThisThread::sleep_for(10ms);//allow value
            state = 5;   
        }
    }
    if (sorted == tubes[1].size  && tubes[1].full == false) {
        sprintf(messages,"Hopper 2 is full!\n");
        np.printf(messages);
        MSG = true;
        TUBE_FULL = true;
        tubes[1].full = true;
        sorted = 0;
        if (tubes_required > 2)
        {
            float initialPos = np.GuideCounter();
            ThisThread::sleep_for(4s);//allow pug to bre funnel into hopper
            np.Guide(40);
            while (np.GuideCounter() <= initialPos + float(41.6)) {//perfect value == 41.5
                ThisThread::sleep_for(10ms);
            }
            np.Guide(0);
            }
        else  
        {
            sprintf(messages,"Task Complete!\n");
            np.printf(messages);
             
            MSG = true;
            DONE = true;
            ThisThread::sleep_for(10ms);//allow value
            state = 5;   
        }
    }
    if (sorted == tubes[2].size && tubes[2].full == false) {
        sprintf(messages,"Hopper 3 is full!\nInsert empty Hoppers\n");
        np.printf(messages);
        MSG = true;
        TUBE_FULL = true;
        tubes[2].full = true;
        sorted = 0;
        DONE = true;
        ThisThread::sleep_for(10ms);//allow value
        state = 5;
        
    }
    
}

void init_tube(int fill = 8)
{
    
    for(int i =0;i < 3;i++)
    {
        //tubes[i].tube_number = i+1;
        tubes[i].empty = true;
        tubes[i].full = false;
        if(fill <= 10 && fill >= 0)
        {
            tubes[i].size = fill;
        }
        
    }
    
}//end func

void buttonhandler()
{
    //Handle mode selector button
    if(modebtn == 0 && mode == production)//off
    {
        mode = test;
    }
    if(modebtn == 1 && mode == test)//on
    {
        mode = production;
    }

   // Handle Pause
    if (pause == 0) { // Button press detected
        if (!paused) {
            run_pause();
        }
    }
    if (mqtt_continue == 1)
    {//Handle Continue
        if(paused)
        {
            run_continue();
            mqtt_continue = 0;
        }
    }
    
    //mobile buttons also
    if(mqtt_pause == 1)
    {
        if(!paused)
        {
            run_pause();
            mqtt_pause = 0;
        }
    }

    if(cont == 0 )
    {
        if(paused)
        {
            run_continue();
        }
    }

    //Handle Estop
    if(estop == 0)//On
    {
        ThisThread::sleep_for(250ms);//allow contact bounce, be sure button is pressed
        if(Estop == 0)
        {
            estop = true;
            if(mode == test)
            {
                test_state = estop_state;
            }
            else if (mode ==production)
            {
                state = estop_state;
            }
        }
    }

}//end func


void run_pause()
{
    // System is currently running, so pause it
    if(mode == test)
    {
        last_state = test_state; // Save the current state
        test_state = pause_state; // Transition to pause state
    }
    else if(mode == production)
    {
        last_state = state; // Save the current state
        state = pause_state; // Transition to pause state
    }
    np.printf("System Paused\n");
    SYS_PAUSED = true;
    paused = true;
}

void run_continue()
{
    // System is currently paused, so continue
    if(mode  == test)
    {
        test_state = last_state; // Resume from the last state
    }
    else if(mode == production)
    {
        state = last_state; // Resume from the last state
    }
    np.printf("System Continuing...\n");
    SYS_CONT = true;
    paused = false;
}


//==========================================MQTT Subscriptions==============================================================
//Message handlers which is triggered when the 'kelvin/PugsGo/...' subscription topics changes
//When a message to pause the system arrives --.../pause
void pauseMsgArrived(MQTT::MessageData& md) {
    int t;
    char *msg = (char*)md.message.payload;      //store the received message into a string (msg)
    msg[md.message.payloadlen] = '\0';          //add NULL to terminate the string based on the message payload length

    t = atoi(msg);                              //convert msg ("0" or "1") to integer 
    mqtt_pause = t;              //write integer value to light output
  
}//end funct

//When message to continue a paused sysyem arrives --.../continue
void continueMsgArrived(MQTT::MessageData& md) {
    int t;
    char *msg = (char*)md.message.payload;      //store the received message into a string (msg)
    msg[md.message.payloadlen] = '\0';          //add NULL to terminate the string based on the message payload length

    t = atoi(msg);                              //convert msg ("0" or "1") to integer 
        mqtt_continue = t; 
    
}//end funct

//When a message to change the speed of the buffer wheel arrives -- .../set/bufferspeed
void speedMsgArrived(MQTT::MessageData& md) {
    int t;
    char *msg = (char*)md.message.payload;      //store the received message into a string (msg)
    msg[md.message.payloadlen] = '\0';          //add NULL to terminate the string based on the message payload length

    t = atoi(msg);                              //convert msg ("0" or "1") to integer 
    
        mqtt_bufferSpeed = t; 
        CURRENT_SPEED = true;
  
  
                             
}//end funct
//When a message to change the recipe/colour sequence arrives --.../set/crecipe
void recipeMsgArrived(MQTT::MessageData& md) {
    int t;
    char *msg = (char*)md.message.payload;      //store the received message into a string (msg)
    msg[md.message.payloadlen] = '\0';          //add NULL to terminate the string based on the message payload length

    t = atoi(msg);                              //convert msg ("0" or "1") to integer 
   
        switch(t)
        {
            case 1:
            {
                rec = mono_r;
            }
            break;
            case 2:
            {
                rec = mono_g;
            }
            break;
            case 3:
            {
                rec = mono_b;
            }
            break;
            case 4:
            {
                rec = bi_rg;
            }
            break;
            case 5:
            {
                rec = bi_rb;
            }
            break;
            case 6:
            {
                rec = bi_gb;
            }
            break;
            case 7:
            {
                rec = tri_rgb;
            }
            break;
            case 8:
            {
                rec = tri_gbr;
            }
            break;
            case 9:
            {
                rec = tri_brg;
            }
            break;
            case 10:
            {
                rec = tri_bgr;
            }
            break;
            case 11:
            {
                rec = tri_rbg;
            }
            break;
            default:
            {
                rec = mono_r;
            }
        }//end switch
        RECIPE_MSG = true;
}//end funct


//When a meesage to adjust the number of pugs required to fill a hopper tube arrives-- .../set/tube_size
void fillMsgArrived(MQTT::MessageData& md) {
    int t;
    char *msg = (char*)md.message.payload;      //store the received message into a string (msg)
    msg[md.message.payloadlen] = '\0';          //add NULL to terminate the string based on the message payload length

    t = atoi(msg);                              //convert msg ("0" or "1") to integer 
  
        mqtt_size = t;                                 
        fill_size = mqtt_size;
    
}//end funct

//When a message to adjust the number fo hopper tubes required to be filled-- .../set/tubes
void tubesMsgArrived(MQTT::MessageData& md) {
    int t;
    char *msg = (char*)md.message.payload;      //store the received message into a string (msg)
    msg[md.message.payloadlen] = '\0';          //add NULL to terminate the string based on the message payload length

    t = atoi(msg);                              //convert msg ("0" or "1") to integer 
   
  
        mqtt_tubes = t;                                  //write integer value to light output
        tubes_required = mqtt_tubes;
    
}//end funct

//User function to publish a MQTT message to a specific topic - pclient is a pointer to the MQTTClient object
int publish(MQTTClient *pclient, const char *topic, const char *msg, int qos, bool retain) {
    MQTT::Message pubMsg;           //create a MQTT Message structure variable
    int ret;

    pubMsg.qos = (MQTT::QoS)qos;            //convert qos from int to MQTT::QoS data type and write to pubMsg.qos
    pubMsg.retained = retain;               //write retain value to pubMsg.retained   
    pubMsg.dup = false;                     //message is not a duplicate
    pubMsg.payload = (void*)msg;            //store string in message structure
    pubMsg.payloadlen = strlen(msg);        //store length of string in message structure
    ret = pclient->publish(topic, pubMsg);  //publish message to MQTTClient pointer
    if (ret != 0) {
        return -1;                          //not successfull and exit function
    }
    return 0;                               //successfull
}//end funct

//Function to handle all TCP/Wifi and MQTT communications
//When a message to change the recipe/colour sequence arrives --.../set/crecipe
void mqtt_wifi()
{
    int ret;                        //general integer variable to use for return values from network APIs
    char buf[40];                   //temporary string
    bool CONNECTED = false;

    TCPSocket client;               //object for the TCP Client (NUCLEO) socket
    SocketAddress brokerAddress;    //class object for broker (server) socket address

    
    
    // ===== Connect WiFi =====
    ret = wifi.connect("Simply WIFI F4", "123456789", NSAPI_SECURITY_WPA2);  //connect to wifi access point (AP) (wifi router)
    if(ret != 0) {                                                          //could not connect to AP, exit program
        np.printf("ESP8266 could not connect.  Error %d\n", ret);
        exit(1);    //stop the program
    }
    np.printf("Connected to WiFi AP...\n");            //succesfully connected to AP

     // ===== Create a TCP socket =====
    client.open(&wifi);                             //create the TCP Client socket using the ESP8266 interface   

    // ===== Connect to broker via TCP socket =====
    wifi.gethostbyname("broker.hivemq.com", &brokerAddress);    //get current ip address for broker (broker.hivemq.com)
    brokerAddress.set_port(1883);                               //set broker port - broker.hivemq.com uses 1883
    np.printf("Broker IP: %s\n", brokerAddress.get_ip_address());  //display broker IP
    ret = client.connect(brokerAddress);                        //make TCP socket connection with broker
    if (ret != 0) { 
        np.printf("Cannot establish TCP socket connection!\n");
        exit(1);    //stop the program
    }
    else 
        np.printf("TCP socket established...\n");

    // ===== Establish MQTT Connection ===== 
    MQTTClient mqttClient(&client);                                     //create MQTTClient object using the established TCP socket
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;   //create a connection data structure variable; load default values into structure
    data.MQTTVersion = 3;                                               //3 = ver 3.1; 4 = ver 3.1.1 (default=4)
    data.clientID.cstring = (char*)"anyID";                             //set MQTT client ID
    data.willFlag = 1;                                                  //enable Last Will & Testament (LWT)
    data.will.topicName.cstring = (char*)"Kelvin/PugsGo/msg";      //topic used from LWT 
    data.will.qos = 2;                                                  //LWT QOS = 1
    data.will.retained = 1;                                             //retain LWT message
    data.will.message.cstring = (char*)"NUCLEO is Live!";                       //LWT message
    ret = mqttClient.connect(data);                                     //connect to the MQTT broker
    if (ret != 0) {
        np.printf("Cannot establish MQTT connection!\n");
        exit(1);    //stop the program
    }
    else {
        np.printf("MQTT connection established...\n");
        CONNECTED = true;
        //status_leds = 0b10101011;
    }

    sprintf(buf, "1");            
    ret = publish(&mqttClient, "Kelvin/PugsGo/nucleo", buf, 0, true);   
    if (ret != 0) {
        np.printf("Cannot publish to 'Kelvin/PugsGo/nucleo'\n");
        CONNECTED = false;
    }//send nucleo msg
        // ===== MQTT Topic subscriptions =====
    //subscribe to 'Tom/house/room1/fan' topic; use QOS0; trigger fanMsgArrived() function when topic changes
    ret = mqttClient.subscribe("Kelvin/PugsGo/pause", MQTT::QOS0, pauseMsgArrived);   
    if (ret != 0)
        np.printf("Cannot subscribe to 'Kelvin/PugsGo/pause' topic!\n");

    //subscribe to 'Tom/house/room1/light' topic; use QOS0; trigger lightMsgArrived() function when topic changes
    ret = mqttClient.subscribe("Kelvin/PugsGo/continue", MQTT::QOS0, continueMsgArrived);   
    if (ret != 0)
        np.printf("Cannot subscribe to 'Kelvin/PugsGo/continue' topic!\n");

 

    //subscribe to 'Tom/house/room1/light' topic; use QOS0; trigger lightMsgArrived() function when topic changes
    ret = mqttClient.subscribe("Kelvin/PugsGo/set/crecipe", MQTT::QOS0, recipeMsgArrived);   
    if (ret != 0)
        np.printf("Cannot subscribe to 'Kelvin/PugsGo/set/crecipe' topic!\n");

    //subscribe to 'Tom/house/room1/light' topic; use QOS0; trigger lightMsgArrived() function when topic changes
    ret = mqttClient.subscribe("Kelvin/PugsGo/set/tubes", MQTT::QOS0, tubesMsgArrived);   
    if (ret != 0)
        np.printf("Cannot subscribe to 'Kelvin/PugsGo/set/tubes' topic!\n");



    //subscribe to 'Tom/house/room1/light' topic; use QOS0; trigger lightMsgArrived() function when topic changes
    ret = mqttClient.subscribe("Kelvin/PugsGo/set/tube_size", MQTT::QOS0, fillMsgArrived);   
    if (ret != 0)
        np.printf("Cannot subscribe to 'Kelvin/PugsGo/set/tube_size' topic!\n");


    //reset all flags
    sprintf(buf, "0");            
            ret = publish(&mqttClient, "Kelvin/PugsGo/alert", buf, 0, true);   
            if (ret != 0) {
                np.printf("Cannot publish to 'Kelvin/PugsGo/alert'\n");
                CONNECTED = false;
            }//send recipe msg

    
            
    while(1)
    {
        if( TEST_COMPLETE)
        {
            sprintf(buf, "Test Complete!");            
            ret = publish(&mqttClient, "Kelvin/PugsGo/msg", buf, 0, true);   
            if (ret != 0) {
                np.printf("Cannot publish to 'Kelvin/PugsGo/msg'\n");
                CONNECTED = false;
            }//send loaded msg
            TEST_COMPLETE = false;
        }

        if( MSG)
        {
                    
            ret = publish(&mqttClient, "Kelvin/PugsGo/msg", messages, 0, true);   
            if (ret != 0) {
                np.printf("Cannot publish to 'Kelvin/PugsGo/msg'\n");
                CONNECTED = false;
            }//send loaded msg
            MSG = false;
        }

        if( SYS_PAUSED)
        {
            sprintf(buf, "System Paused...\n");
            ret = publish(&mqttClient, "Kelvin/PugsGo/msg", buf, 0, true);   
            if (ret != 0) {
                np.printf("Cannot publish to 'Kelvin/PugsGo/msg'\n");
                CONNECTED = false;
            }//send loaded msg
            SYS_PAUSED = false;
        }

        if( SYS_CONT)
        {
            sprintf(buf, "System Continuing...\n");
            ret = publish(&mqttClient, "Kelvin/PugsGo/msg", buf, 0, true);   
            if (ret != 0) {
                np.printf("Cannot publish to 'Kelvin/PugsGo/msg'\n");
                CONNECTED = false;
            }//send loaded msg
            SYS_CONT = false;
        }

        if(TUBE_FULL)
        {
            tube_filled ++;
            sprintf(buf, "%d",tube_filled);         
            ret = publish(&mqttClient, "Kelvin/PugsGo/tubes", buf, 0, true);   
            if (ret != 0) {
                np.printf("Cannot publish to 'Kelvin/PugsGo/tubes'\n");
                CONNECTED = false;
            }//send loaded msg
            TUBE_FULL = false;
        }
        if( TOTAL_LOAD)
        {
            
            sprintf(buf, "%d",total_loaded); 
            ret = publish(&mqttClient, "Kelvin/PugsGo/stats/loaded", buf, 0, true);   
            if (ret != 0) {
                np.printf("Cannot publish to 'Kelvin/PugsGo/stats/load'\n");
                CONNECTED = false;
            }//send loaded msg
            TOTAL_LOAD = false;
        }

        if( TOTAL_PAINT)
        {
           
            sprintf(buf, "%d",total_painted);         
            ret = publish(&mqttClient, "Kelvin/PugsGo/stats/painted", buf, 0, true);   
            if (ret != 0) {
                np.printf("Cannot publish to 'Kelvin/PugsGo/stats/painted'\n");
                CONNECTED = false;
            }//send loaded msg
            TOTAL_PAINT = false;
        }

        if( TOTAL_SORT)
        {
            //total_sorted = total_sorted + sorted;
            sprintf(buf, "%d",total_sorted);         
            ret = publish(&mqttClient, "Kelvin/PugsGo/stats/sorted", buf, 0, true);   
            if (ret != 0) {
                np.printf("Cannot publish to 'Kelvin/PugsGo/stats/sorted'\n");
                CONNECTED = false;
            }//send loaded msg
            TOTAL_SORT = false;
        }

        if( TOTAL_LOST)
        {
         
            sprintf(buf, "%d",total_lost_pugs);         
            ret = publish(&mqttClient, "Kelvin/PugsGo/stats/lost", buf, 0, true);   
            if (ret != 0) {
                np.printf("Cannot publish to 'Kelvin/PugsGo/stats/lost'\n");
                CONNECTED = false;
            }//send loaded msg
            TOTAL_LOST = false;
        }

        if( TOTAL_UNPAINT)
        {
           
            sprintf(buf, "%d",total_unpaint);         
            ret = publish(&mqttClient, "Kelvin/PugsGo/stats/unpainted", buf, 0, true);   
            if (ret != 0) {
                np.printf("Cannot publish to 'Kelvin/PugsGo/stats/unpainted'\n");
                CONNECTED = false;
            }//send loaded msg
            TOTAL_UNPAINT = false;
        }


        if( BOX_LOADED)
        {
            sprintf(buf, "%d",load_count);            
            ret = publish(&mqttClient, "Kelvin/PugsGo/loaded", buf, 0, true);   
            if (ret != 0) {
                np.printf("Cannot publish to 'Kelvin/PugsGo/loaded'\n");
                CONNECTED = false;
            }//send loaded msg
            BOX_LOADED = false;
        }
        if( STAGE)
        {
            sprintf(buf, "%d",stage);            
            ret = publish(&mqttClient, "Kelvin/PugsGo/stage", buf, 0, true);   
            if (ret != 0) {
                np.printf("Cannot publish to 'Kelvin/PugsGo/stage'\n");
                CONNECTED = false;
            }//send stage msg
            STAGE =  false;
        }
        if( BOX_PAINTED)
        {
            sprintf(buf, "%d",paint_count);            
            ret = publish(&mqttClient, "Kelvin/PugsGo/painted", buf, 0, true);   
            if (ret != 0) {
                np.printf("Cannot publish to 'Kelvin/PugsGo/painted'\n");
                CONNECTED = false;
            }//send painted msg
            BOX_PAINTED = false;
        }
        if( BOX_SORTED)
        {
            sprintf(buf, "%d",sorted);            
            ret = publish(&mqttClient, "Kelvin/PugsGo/sorted", buf, 0, true);   
            if (ret != 0) {
                np.printf("Cannot publish to 'Kelvin/PugsGo/sorted'\n");
                CONNECTED = false;
            }//send sorted msg

            BOX_SORTED = false;
        }
        if( RED)
        {
            sprintf(buf, "%d",r);            
            ret = publish(&mqttClient, "Kelvin/PugsGo/red", buf, 0, true);   
            if (ret != 0) {
                np.printf("Cannot publish to 'Kelvin/PugsGo/red'\n");
                CONNECTED = false;
            }//send red msg
            sprintf(buf, "%d",total_r);            
            ret = publish(&mqttClient, "Kelvin/PugsGo/stats/red", buf, 0, true);   
            if (ret != 0) {
                np.printf("Cannot publish to 'Kelvin/PugsGo/stats/red'\n");
                CONNECTED = false;
            }//send red msg
            RED = false;
        }
        if( GREEN)
        {
            sprintf(buf, "%d",g);            
            ret = publish(&mqttClient, "Kelvin/PugsGo/green", buf, 0, true);   
            if (ret != 0) {
                np.printf("Cannot publish to 'Kelvin/PugsGo/green'\n");
                CONNECTED = false;
            }//send green msg
            sprintf(buf, "%d",total_g);            
            ret = publish(&mqttClient, "Kelvin/PugsGo/stats/green", buf, 0, true);   
            if (ret != 0) {
                np.printf("Cannot publish to 'Kelvin/PugsGo/stats/green'\n");
                CONNECTED = false;
            }//send red msg
            GREEN = false;
        }

        if( BLUE)
        {
            sprintf(buf, "%d",b);            
            ret = publish(&mqttClient, "Kelvin/PugsGo/blue", buf, 0, true);   
            if (ret != 0) {
                np.printf("Cannot publish to 'Kelvin/PugsGo/blue'\n");
                CONNECTED = false;
            }//send blue msg
            sprintf(buf, "%d",total_b);            
            ret = publish(&mqttClient, "Kelvin/PugsGo/stats/blue", buf, 0, true);   
            if (ret != 0) {
                np.printf("Cannot publish to 'Kelvin/PugsGo/stats/blue'\n");
                CONNECTED = false;
            }//send red msg
            BLUE =  false;
        }

        if( BOX_UNPAINTED)
        {
            sprintf(buf, "%d",unpainted);            
            ret = publish(&mqttClient, "Kelvin/PugsGo/unpainted", buf, 0, true);   
            if (ret != 0) {
                np.printf("Cannot publish to 'Kelvin/PugsGo/unpainted'\n");
                CONNECTED = false;
            }//send unpainted msg
            BOX_UNPAINTED = false;
        }

        if( TOTAL_RED)
        {
            sprintf(buf, "%d",total_r);            
            ret = publish(&mqttClient, "Kelvin/PugsGo/stats/red", buf, 0, true);   
            if (ret != 0) {
                np.printf("Cannot publish to 'Kelvin/PugsGo/stats/red'\n");
                CONNECTED = false;
            }//send unpainted msg
            TOTAL_RED = false;
        }

        if( TOTAL_GREEN)
        {
            sprintf(buf, "%d",total_g);            
            ret = publish(&mqttClient, "Kelvin/PugsGo/stats/green", buf, 0, true);   
            if (ret != 0) {
                np.printf("Cannot publish to 'Kelvin/PugsGo/stats/green'\n");
                CONNECTED = false;
            }//send unpainted msg
            TOTAL_GREEN = false;
        }
        
        if( TOTAL_BLUE)
        {
            sprintf(buf, "%d",total_b);            
            ret = publish(&mqttClient, "Kelvin/PugsGo/stats/blue", buf, 0, true);   
            if (ret != 0) {
                np.printf("Cannot publish to 'Kelvin/PugsGo/stats/blue'\n");
                CONNECTED = false;
            }//send unpainted msg
            TOTAL_BLUE = false;
        }
        
        
        if( BOX_LOST)
        {
            sprintf(buf, "%d",lost);            
            ret = publish(&mqttClient, "Kelvin/PugsGo/lost", buf, 0, true);   
            if (ret != 0) {
                np.printf("Cannot publish to 'Kelvin/PugsGo/lost'\n");
                CONNECTED = false;
            }//send lost msg
            BOX_LOST = false;
        }

        if( SPEED_MSG)
        {
            sprintf(buf, "%d",speed);            
            ret = publish(&mqttClient, "Kelvin/PugsGo/bufferspeed", buf, 0, true);   
            if (ret != 0) {
                np.printf("Cannot publish to 'Kelvin/PugsGo/bufferspeed'\n");
                CONNECTED = false;
            }//send speed msg
            SPEED_MSG = false;
        }

        if( RECIPE_MSG)
        {
            switch(rec)
            {
                case mono_r:
                {
                    strcpy(buf,"mono_r");          
                    ret = publish(&mqttClient, "Kelvin/PugsGo/recipe", buf, 0, true);   
                    if (ret != 0) {
                        np.printf("Cannot publish to 'Kelvin/PugsGo/recipe'\n");
                        CONNECTED = false;
                    }//send recipe msg
                    cr ++;
                    sprintf(buf,"%d",cr);          
                    ret = publish(&mqttClient, "Kelvin/PugsGo/stats/r", buf, 0, true);   
                    if (ret != 0) {
                        np.printf("Cannot publish to 'Kelvin/PugsGo/stats/r'\n");
                        CONNECTED = false;
                    }//send recipe msg
                    RECIPE_MSG = false;
                }
                break;
                case mono_g:
                {
                    strcpy(buf,"mono_g");          
                    ret = publish(&mqttClient, "Kelvin/PugsGo/recipe", buf, 0, true);   
                    if (ret != 0) {
                        np.printf("Cannot publish to 'Kelvin/PugsGo/recipe'\n");
                        CONNECTED = false;
                    }//send recipe msg
                    cg ++;
                    sprintf(buf,"%d",cg);          
                    ret = publish(&mqttClient, "Kelvin/PugsGo/stats/g", buf, 0, true);   
                    if (ret != 0) {
                        np.printf("Cannot publish to 'Kelvin/PugsGo/stats/g'\n");
                        CONNECTED = false;
                    }//send recipe msg
                    RECIPE_MSG = false;
                }
                break;
                case mono_b:
                {
                    strcpy(buf,"mono_b");          
                    ret = publish(&mqttClient, "Kelvin/PugsGo/recipe", buf, 0, true);   
                    if (ret != 0) {
                        np.printf("Cannot publish to 'Kelvin/PugsGo/recipe'\n");
                        CONNECTED = false;
                    }//send recipe msg
                    cb ++;
                    sprintf(buf,"%d",cb);          
                    ret = publish(&mqttClient, "Kelvin/PugsGo/stats/b", buf, 0, true);   
                    if (ret != 0) {
                        np.printf("Cannot publish to 'Kelvin/PugsGo/stats/b'\n");
                        CONNECTED = false;
                    }//send recipe msg
                    RECIPE_MSG = false;
                }
                break;
                case bi_rg:
                {
                    strcpy(buf,"bi_rg");          
                    ret = publish(&mqttClient, "Kelvin/PugsGo/recipe", buf, 0, true);   
                    if (ret != 0) {
                        np.printf("Cannot publish to 'Kelvin/PugsGo/recipe'\n");
                        CONNECTED = false;
                    }//send recipe msg
                    crg ++;
                    sprintf(buf,"%d",crg);          
                    ret = publish(&mqttClient, "Kelvin/PugsGo/stats/rg", buf, 0, true);   
                    if (ret != 0) {
                        np.printf("Cannot publish to 'Kelvin/PugsGo/stats/rg'\n");
                        CONNECTED = false;
                    }//send recipe msg
                    RECIPE_MSG = false;
                }
                break;
                case bi_gb:
                {
                    strcpy(buf,"bi_gb");          
                    ret = publish(&mqttClient, "Kelvin/PugsGo/recipe", buf, 0, true);   
                    if (ret != 0) {
                        np.printf("Cannot publish to 'Kelvin/PugsGo/recipe'\n");
                        CONNECTED = false;
                    }//send recipe msg
                    cgb ++;
                    sprintf(buf,"%d",cgb);          
                    ret = publish(&mqttClient, "Kelvin/PugsGo/stats/gb", buf, 0, true);   
                    if (ret != 0) {
                        np.printf("Cannot publish to 'Kelvin/PugsGo/stats/gb'\n");
                        CONNECTED = false;
                    }//send recipe msg
                    RECIPE_MSG = false;
                }
                break;
                case bi_rb:
                {
                    strcpy(buf,"bi_rb");          
                    ret = publish(&mqttClient, "Kelvin/PugsGo/recipe", buf, 0, true);   
                    if (ret != 0) {
                        np.printf("Cannot publish to 'Kelvin/PugsGo/recipe'\n");
                        CONNECTED = false;
                    }//send recipe msg
                    crb ++;
                    sprintf(buf,"%d",crb);          
                    ret = publish(&mqttClient, "Kelvin/PugsGo/stats/rb", buf, 0, true);   
                    if (ret != 0) {
                        np.printf("Cannot publish to 'Kelvin/PugsGo/stats/rb'\n");
                        CONNECTED = false;
                    }//send recipe msg
                    RECIPE_MSG = false;
                }
                break;
                case tri_rgb:
                {
                    strcpy(buf,"tri_rgb");          
                    ret = publish(&mqttClient, "Kelvin/PugsGo/recipe", buf, 0, true);   
                    if (ret != 0) {
                        np.printf("Cannot publish to 'Kelvin/PugsGo/recipe'\n");
                        CONNECTED = false;
                    }//send recipe msg
                    crgb ++;
                    sprintf(buf,"%d",crgb);          
                    ret = publish(&mqttClient, "Kelvin/PugsGo/stats/rgb", buf, 0, true);   
                    if (ret != 0) {
                        np.printf("Cannot publish to 'Kelvin/PugsGo/stats/rgb'\n");
                        CONNECTED = false;
                    }//send recipe msg
                    RECIPE_MSG = false;
                }
                break;
                case tri_rbg:
                {
                    strcpy(buf,"tri_rbg");          
                    ret = publish(&mqttClient, "Kelvin/PugsGo/recipe", buf, 0, true);   
                    if (ret != 0) {
                        np.printf("Cannot publish to 'Kelvin/PugsGo/recipe'\n");
                        CONNECTED = false;
                    }//send recipe msg
                    crbg ++;
                    sprintf(buf,"%d",crbg);          
                    ret = publish(&mqttClient, "Kelvin/PugsGo/stats/rbg", buf, 0, true);   
                    if (ret != 0) {
                        np.printf("Cannot publish to 'Kelvin/PugsGo/stats/rbg'\n");
                        CONNECTED = false;
                    }//send recipe msg
                    RECIPE_MSG = false;
                }
                break;
                case tri_gbr:
                {
                    strcpy(buf,"tri_gbr");          
                    ret = publish(&mqttClient, "Kelvin/PugsGo/recipe", buf, 0, true);   
                    if (ret != 0) {
                        np.printf("Cannot publish to 'Kelvin/PugsGo/recipe'\n");
                        CONNECTED = false;
                    }//send recipe msg
                    cgbr ++;
                    sprintf(buf,"%d",cgbr);          
                    ret = publish(&mqttClient, "Kelvin/PugsGo/stats/gbr", buf, 0, true);   
                    if (ret != 0) {
                        np.printf("Cannot publish to 'Kelvin/PugsGo/stats/gbr'\n");
                        CONNECTED = false;
                    }//send recipe msg
                    RECIPE_MSG = false;
                }
                break;
                case tri_bgr:
                {
                    strcpy(buf,"tir_bgr");          
                    ret = publish(&mqttClient, "Kelvin/PugsGo/recipe", buf, 0, true);   
                    if (ret != 0) {
                        np.printf("Cannot publish to 'Kelvin/PugsGo/recipe'\n");
                        CONNECTED = false;
                    }//send recipe msg
                    cbgr ++;
                    sprintf(buf,"%d",cbgr);          
                    ret = publish(&mqttClient, "Kelvin/PugsGo/stats/bgr", buf, 0, true);   
                    if (ret != 0) {
                        np.printf("Cannot publish to 'Kelvin/PugsGo/stats/bgr'\n");
                        CONNECTED = false;
                    }//send recipe msg
                    RECIPE_MSG = false;
                }
                break;
                case tri_brg:
                {
                    strcpy(buf,"tri_brg");          
                    ret = publish(&mqttClient, "Kelvin/PugsGo/recipe", buf, 0, true);   
                    if (ret != 0) {
                        np.printf("Cannot publish to 'Kelvin/PugsGo/recipe'\n");
                        CONNECTED = false;
                    }//send recipe msg
                    cbrg ++;
                    sprintf(buf,"%d",cbrg);          
                    ret = publish(&mqttClient, "Kelvin/PugsGo/stats/brg", buf, 0, true);   
                    if (ret != 0) {
                        np.printf("Cannot publish to 'Kelvin/PugsGo/stats/brg'\n");
                        CONNECTED = false;
                    }//send recipe msg
                    RECIPE_MSG = false;
                }
                break;
                default:
                {
                    strcpy(buf,"unkown recipe");          
                    ret = publish(&mqttClient, "Kelvin/PugsGo/recipe", buf, 0, true);   
                    if (ret != 0) {
                        np.printf("Cannot publish to 'Kelvin/PugsGo/recipe'\n");
                        CONNECTED = false;
                    }//send recipe msg
                    RECIPE_MSG = false;
                }

            }//end switch
        }



        if( E_STOPPED)//simulate alert flashing
        {
            
            
            sprintf(buf, "EMERGENCY!");            
            ret = publish(&mqttClient, "Kelvin/PugsGo/msg", buf, 0, true);  
            if (ret != 0) {
                np.printf("Cannot publish to 'Kelvin/PugsGo/msg'\n");
                CONNECTED = false;
            }//send recipe msg
            while(E_STOPPED)
            {
                sprintf(buf, "1");            
                ret = publish(&mqttClient, "Kelvin/PugsGo/alert", buf, 0, true);   
                if (ret != 0) {
                    np.printf("Cannot publish to 'Kelvin/PugsGo/alert'\n");
                    CONNECTED = false;
                }//send recipe msg
                ThisThread::sleep_for(1000ms);
                sprintf(buf, "0");            
                ret = publish(&mqttClient, "Kelvin/PugsGo/alert", buf, 0, true);   
                if (ret != 0) {
                    np.printf("Cannot publish to 'Kelvin/PugsGo/alert'\n");
                    CONNECTED = false;
                }//send recipe msg
            }
            E_STOPPED = false;
            
        }
        // ===== Ping MQTT broker and allow library to complete internal tasks =====
        mqttClient.yield(1000);
        sprintf(buf, "1");            
        ret = publish(&mqttClient, "Kelvin/PugsGo/nucleo", buf, 0, true);   
        if (ret != 0) {
            np.printf("Cannot publish to 'Kelvin/PugsGo/nucleo'\n");
            CONNECTED = false;
        }//send nucleo msg
        

         // ===== Test if connection is still alive.  If not, try to connect again =====
        if (CONNECTED == false) {
            ret = mqttClient.connect(data);                                     //connect to the MQTT broker
            if (ret != 0) {
                np.printf("Cannot establish MQTT connection!\n");
            }
            else {
               np.printf("MQTT connection established...\n");
                CONNECTED = true;
            }
        }
        
    }
}
//===================================================The End===========================================================


