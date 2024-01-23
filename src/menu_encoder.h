#ifndef MENU_ENCODER_H
#define MENU_ENCODER_H

#ifdef __cplusplus
extern "C"
{
#endif

    // Variables internals

    long menu_encoder_init_value = 0;
    long old_position = 0;

    long menu_encoder_last_movement = 0;
    long menu_encoder_timeout = 1000; // 500 millis sense moviment produeix submit

    bool menu_encoder_update(void (*onChange)(long), void (*onSubmit)(long))
    {
        long newPosition = M5Dial.Encoder.read();
        if (newPosition != old_position)
        { // We have movement
            M5Dial.Speaker.tone(3000, 30, 0, true);
            if (menu_encoder_last_movement == 0)
            { // First Movement
                USBSerial.println("Starting Encoder");
                menu_encoder_init_value = newPosition;
            }
            else
            {
                onChange(newPosition - menu_encoder_init_value);
            }
            menu_encoder_last_movement = millis();
            old_position = newPosition;
            last_touched = millis();
            return true;
        }
        else if (menu_encoder_last_movement != 0)
        {
            if ((millis() - menu_encoder_last_movement) > menu_encoder_timeout)
            {
                USBSerial.println("Calling Submit");               
                onSubmit(old_position - menu_encoder_init_value);
                menu_encoder_last_movement = 0;
                return true;
            }
        }
        old_position = newPosition;
        return false;
    }

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
