#include "swipe_input.h"

namespace SwipeInput {

namespace {
    // Ab dieser Verschiebung (in beide Achsen, Pixel) gilt eine
    // Beruehrung als Wisch-Geste statt als Tap - bewusst grosszuegig
    // bemessen, damit ein leicht zittriger Tap (z.B. auf den
    // "?"-Infobutton) nicht faelschlich als Wisch gewertet wird.
    constexpr int16_t SWIPE_THRESHOLD_PX = 40;

    bool tracking = false;
    TouchInput::Point downPoint;
}

bool poll(Direction& dir, TouchInput::Point& outPoint) {
    TouchInput::Point cur = TouchInput::mappedPoint();
    if (cur.touched && !tracking) {
        tracking = true;
        downPoint = cur;
    }

    TouchInput::Point released;
    if (!TouchInput::wasTapped(released)) return false;

    tracking = false;
    outPoint = released;

    int16_t dx = (int16_t)(released.x - downPoint.x);
    int16_t dy = (int16_t)(released.y - downPoint.y);
    int16_t adx = (int16_t)abs(dx);
    int16_t ady = (int16_t)abs(dy);

    if (adx < SWIPE_THRESHOLD_PX && ady < SWIPE_THRESHOLD_PX) {
        dir = Direction::None;
        return true;
    }

    if (adx > ady) {
        dir = (dx < 0) ? Direction::Left : Direction::Right;
    } else {
        dir = (dy < 0) ? Direction::Up : Direction::Down;
    }
    return true;
}

}
