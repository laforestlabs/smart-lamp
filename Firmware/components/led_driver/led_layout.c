#include "led_driver.h"

/*
 * Physical (col, row) position of each LED in the 7-row oval grid.
 * Index 0 = D1, index 30 = D31.  See spec §2.5.
 *
 * Column:   0      1      2      3      4
 * Row 0:         [ D1]  [ D2]  [ D3]
 * Row 1:  [ D4]  [ D5]  [ D6]  [ D7]  [ D8]
 * Row 2:  [ D9]  [D10]  [D11]  [D12]  [D13]
 * Row 3:  [D14]  [D15]  [D16]  [D17]  [D18]
 * Row 4:  [D19]  [D20]  [D21]  [D22]  [D23]
 * Row 5:  [D24]  [D25]  [D26]  [D27]  [D28]
 * Row 6:         [D29]  [D30]  [D31]
 */
const led_coord_t led_coords[LED_COUNT] = {
    /* Row 0 (D1–D3)  */ {1, 0}, {2, 0}, {3, 0},
    /* Row 1 (D4–D8)  */ {0, 1}, {1, 1}, {2, 1}, {3, 1}, {4, 1},
    /* Row 2 (D9–D13) */ {0, 2}, {1, 2}, {2, 2}, {3, 2}, {4, 2},
    /* Row 3 (D14–D18)*/ {0, 3}, {1, 3}, {2, 3}, {3, 3}, {4, 3},
    /* Row 4 (D19–D23)*/ {0, 4}, {1, 4}, {2, 4}, {3, 4}, {4, 4},
    /* Row 5 (D24–D28)*/ {0, 5}, {1, 5}, {2, 5}, {3, 5}, {4, 5},
    /* Row 6 (D29–D31)*/ {1, 6}, {2, 6}, {3, 6},
};
