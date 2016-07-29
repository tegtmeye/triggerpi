/*
    Abstract class for an ADC board
 */

#ifndef ADC_BOARD_H
#define ADC_BOARD_H

class ADC_board {
  public:
    virtual void setup_com(void) = 0;
//     virtual void initialize(void) = 0;
};

#endif
