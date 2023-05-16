#include "CTS_Layer.h"
#include <limits.h>

#define RED 0
#define BLUE 1



int Tap_or_Hold();
int minimax();
int Check_Win(unsigned int current_ticks);
void Welcome_Sequence();
void Win_Sequence();
void Tie_Sequence();
int minimax(unsigned int red_tics, unsigned int blue_tics, unsigned int color_turn);
void ai_player_move(unsigned int players_tics_copy[]);
unsigned int get_led_port(unsigned int location, unsigned int color);
unsigned int get_led_bit(unsigned int location, unsigned int color);
void led_on(unsigned int location_num, unsigned int color);
void led_off(unsigned int location_num, unsigned int color);
void serial_set_leds(unsigned int serial_locations, unsigned int color);
void led_flip(unsigned int location_num, unsigned int color);

const unsigned int WIN_PATTERN[]   = {0b000000111, 0b000111000, 0b111000000, 0b001001001, 0b010010010, 0b100100100, 0b100010001, 0b001010100};
unsigned int curr_color = RED, players_tics[2]; // = {{RED LEDs}, {BLUE LEDs}}


void main(void)
{ 
    // Initialise System Clocks
    WDTCTL = WDTPW + WDTHOLD;             // Stop watch-dog timer
    BCSCTL1 = CALBC1_1MHZ;                // Set DCO to 1, 8, 12 or 16MHz
    DCOCTL = CALDCO_1MHZ;
    BCSCTL3 |= LFXT1S_2;                  // LFXT1 = VLO

    P1DIR |= BIT0 | BIT1 | BIT2 | BIT3 | BIT4 | BIT5;

    P1DIR &= ~BIT6;
    P1REN |= BIT6;
    P1OUT |= BIT6;

    P2DIR |= BIT0 | BIT1 | BIT2 | BIT3 | BIT4 | BIT5;
    P3DIR |= BIT0 | BIT1 | BIT2 | BIT4 | BIT5 | BIT6;

    P1OUT = 0;
    P2OUT = 0;
    P3OUT = 0;

    unsigned int i=0, empty_locations;
    int selection=0;

    Welcome_Sequence();

    TI_CAPT_Init_Baseline(&one_button);         // Initialise Baseline measurement
    TI_CAPT_Update_Baseline(&one_button, 5);    // Update baseline measurement (Average 5 measurements)


    while(1) // Main loop
    {
        players_tics[RED] = players_tics[BLUE] = i = 0;
        serial_set_leds(0, RED);
        serial_set_leds(0, BLUE);

        while (1) // Game loop
        {

            if((curr_color == BLUE && (P1IN & BIT6))){
                ai_player_move(players_tics);
            }else{
                // Find an empty tick spot to propose to player
                if(i >= 9){
                    i = 0;
                }
                empty_locations = ~(players_tics[RED] | players_tics[BLUE]);
                for(; !((1 << i) & empty_locations); i++);

                for(selection = Tap_or_Hold(); selection == -1; selection = Tap_or_Hold()){
                    led_flip(i, curr_color);
                    __delay_cycles(60000);
                }
                // selection=0 -> short tap, selection=1 -> long press
                if(!selection){
                    led_off(i, curr_color);
                    i++;
                    continue;
                }

                // Record and render player selection
                players_tics[curr_color] |= (1 << i);
                led_on(i, curr_color);
            }

            // Check if player win
            if(Check_Win(players_tics[curr_color])){
                __delay_cycles(1000000);
                Win_Sequence();
                break;
            }

            // Check if game is a tie
            if((players_tics[RED] | players_tics[BLUE]) == 0b111111111){
                Tie_Sequence();
                break;
            }

            // Other player's turn
            curr_color ^= 1;
            i=0;

        } // Game loop

    } // Main loop

}




int minimax(unsigned int red_tics, unsigned int blue_tics, unsigned int color_turn){
    unsigned int i, empty_locations;
    int best_score, score;

    if(Check_Win(red_tics)){                    // enemy wins-> -1
        return -1;
    }
    if(Check_Win(blue_tics)){                    // AI wins -> 1
        return 1;
    }
    if((red_tics | blue_tics) == 0b111111111){  // tie -> 0
        return 0;
    }

    empty_locations = ~(red_tics | blue_tics);

    if(color_turn == BLUE){ // Maximising
        best_score=INT_MIN;
        for(i=0;i<9;i++){
            if(((1 << i) & empty_locations)){
                blue_tics |= 1 << i;
                score = minimax(red_tics, blue_tics, color_turn ^ 1);
                blue_tics ^= 1 << i;
                if(score > best_score)
                    best_score = score;
            }
        }
    }else{ // Minimising
        best_score=INT_MAX;
        for(i=0;i<9;i++){
            if(((1 << i) & empty_locations)){
                red_tics |= 1 << i;
                score = minimax(red_tics, blue_tics, color_turn ^ 1);
                red_tics ^= 1 << i;
                if(score < best_score)
                    best_score = score;
            }
        }
    }
    return best_score;
}

void ai_player_move(unsigned int players_tics_copy[]){
    BCSCTL1 = CALBC1_16MHZ; // Ramp up clock frequency for faster performance
    unsigned int i, j, empty_locations, best_move=10, red_tics=players_tics[RED], blue_tics=players_tics[BLUE];
    int best_score = INT_MIN, score;

    empty_locations = ~(red_tics | blue_tics);

    if(~empty_locations == 0){
        best_move = 0;
    }else{
        for(i=0;i<9;i++){
            if(((1 << i) & empty_locations)){   //if location is empty
                blue_tics |= 1 << i;
                 score = minimax(red_tics, blue_tics, RED);
                 blue_tics ^= 1 << i;
                 if(score > best_score){
                     best_score = score;
                     best_move = i;
                 }
            }
        }
    }
    BCSCTL1 = CALBC1_1MHZ;

    // render the selection to LEDs
    for(j=0;j<5;j++){
        led_flip(best_move, BLUE);
        __delay_cycles(60000);
    }
    players_tics[BLUE] |= (1 << best_move);
    led_on(best_move, BLUE);
    __delay_cycles(100000);
}














int Check_Win(unsigned int current_ticks){
    unsigned int i;
    for(i=0; i<8; i++){
        if((current_ticks & WIN_PATTERN[i]) == WIN_PATTERN[i])
            return 1;
    }
    return 0;
}

// returns 1 if button is held, 0 if tapped.
int Tap_or_Hold()
{
    int extend = 0;
    int is_hold = -1;
    if(TI_CAPT_Button(&one_button)){
        __delay_cycles(400000);
        if(TI_CAPT_Button(&one_button)){ // HOLD
            is_hold = 1;
            extend = 1;
        }else{  // TAP
            is_hold = 0;
        }
    }
    while(extend){
        extend = 0;
        __delay_cycles(20000);
        if(TI_CAPT_Button(&one_button)){ extend=1; }
    }
    return is_hold;
}



//****************************************************//
/*                LED sequence functions              */
//****************************************************//

void Welcome_Sequence(){
    const unsigned int PATTERN[]   = {0b100, 0b100110, 0b100110011, 0b110011001, 0b11001000, 0b1000000, 0b100, 0, 0b100110, 0b100110011, 0b110011001, 0b11001000, 0b1000000};
    unsigned int i;
    for(i=0;i<13;i++){
        serial_set_leds(PATTERN[i], (i == 6 || i > 7) ? BLUE : RED);
        __delay_cycles(200000);
    }
    serial_set_leds(0, RED);
    serial_set_leds(0, BLUE);
}

void Win_Sequence(){
    unsigned int i;
    serial_set_leds(0, RED);
    serial_set_leds(0, BLUE);
    for(i=0;i<5;i++){
        serial_set_leds(0b010111101, curr_color);
        __delay_cycles(400000);
        serial_set_leds(0b101111010, curr_color);
        __delay_cycles(400000);
    }
}

void Tie_Sequence(){
    unsigned int i;
    serial_set_leds(0, RED);
    serial_set_leds(0, BLUE);
    for(i=0;i<5;i++){
        serial_set_leds(0b101010101, RED);
        __delay_cycles(400000);
        serial_set_leds(0b000000000, RED);
        __delay_cycles(400000);
    }
}


//****************************************************//
/*              LED manipulation functions            */
//****************************************************//

void serial_set_leds(unsigned int serial_locations, unsigned int color){
    unsigned int i;
    for(i=0;i<9;i++){
        //if( ((1 << i) & serial_locations) != ((1 << i) & players_tics[color]) ){
        //    led_flip(i, color);
        //}

        if((1 << i) & serial_locations){
            led_on(i, color);
        }else{
            led_off(i, color);
        }

    }
}

void led_flip(unsigned int location_num, unsigned int color){
    switch(get_led_port(location_num, color))
    {
    case 1:
        P1OUT ^= get_led_bit(location_num, color);
        break;
    case 2:
        P2OUT ^= get_led_bit(location_num, color);
        break;
    case 3:
        P3OUT ^= get_led_bit(location_num, color);
        break;
    }
}

void led_on(unsigned int location_num, unsigned int color){
    switch(get_led_port(location_num, color))
    {
    case 1:
        P1OUT |= get_led_bit(location_num, color);
        break;
    case 2:
        P2OUT |= get_led_bit(location_num, color);
        break;
    case 3:
        P3OUT |= get_led_bit(location_num, color);
        break;
    }
}

void led_off(unsigned int location_num, unsigned int color){
    switch(get_led_port(location_num, color))
    {
    case 1:
        P1OUT &= ~get_led_bit(location_num, color);
        break;
    case 2:
        P2OUT &= ~get_led_bit(location_num, color);
        break;
    case 3:
        P3OUT &= ~get_led_bit(location_num, color);
        break;
    }
}

unsigned int get_led_port(unsigned int location_num, unsigned int color){
    switch(location_num)
    {
    case 0:
        return 1;
    case 1:
        return 3;
    case 2:
        return color == BLUE ? 3 : 2;
    case 3:
        return 1;
    case 4:
        return 2;
    case 5:
        return 2;
    case 6:
        return 1;
    case 7:
        return color == BLUE ? 2 : 3;
    case 8:
        return 3;
    }
    return 0;
}

unsigned int get_led_bit(unsigned int location_num, unsigned int color){
    switch(location_num)
    {
    case 0:
        return color == BLUE ? BIT0 : BIT1;
    case 1:
        return color == BLUE ? BIT1 : BIT0;
    case 2:
        return color == BLUE ? BIT4 : BIT3;
    case 3:
        return color == BLUE ? BIT2 : BIT3;
    case 4:
        return color == BLUE ? BIT0 : BIT1;
    case 5:
        return color == BLUE ? BIT4 : BIT5;
    case 6:
        return color == BLUE ? BIT4 : BIT5;
    case 7:
        return BIT2;
    case 8:
        return color == BLUE ? BIT5 : BIT6;
    }
    return 0;
}

/*
P3.4    BLUE 2   |   P3.1    BLUE 1   |   P1.0    BLUE 0
P2.3    RED  2   |   P3.0    RED  1   |   P1.1    RED  0
--------------------------------------------------------
P2.4    BLUE 5   |   P2.0    BLUE 4   |   P1.2    BLUE 3
P2.5    RED  5   |   P2.1    RED  4   |   P1.3    RED  3
--------------------------------------------------------
P3.5    BLUE 8   |   P2.2    BLUE 7   |   P1.4    BLUE 6
P3.6    RED  8   |   P3.2    RED  7   |   P1.5    RED  6
*/


#pragma GCC pop_options

