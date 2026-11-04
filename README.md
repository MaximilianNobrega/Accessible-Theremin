# Accessible Theremin
**Embedded Systems | Arduino | C++ | Real-Time Audio Processing**

## Project Overview

The Accessible Theremin is a touchless electronic musical instrument designed to make pitch control more intuitive for beginner musicians.

The instrument uses two ultrasonic distance sensors to independently control pitch and volume through hand movements. A 16×2 LCD displays the generated frequency in hertz (Hz), providing visual feedback for users who may have difficulty identifying musical pitches by ear.

Developed as part of a four-person engineering team, the project combined embedded software, sensor integration, digital audio generation, and analog audio circuitry.

I served as the primary embedded software developer, writing and refining most of the Arduino C++ firmware and contributing to hardware integration and testing.

## Key Features

- Touchless pitch and volume control using two ultrasonic distance sensors
- Real-time frequency display on a 16×2 I²C LCD
- Audio generation using the Mozzi Arduino audio library
- Smooth pitch transitions through software-based frequency interpolation
- External audio amplification and speaker output
- Sensor operation over an approximately 0–60 cm hand-distance range

## My Contributions

### Embedded Software Development
- Led firmware development for the four-person project team.
- Implemented ultrasonic sensor measurements and distance-to-frequency mapping.
- Integrated the Mozzi audio library to generate continuous audio output.
- Developed software-based smoothing to reduce abrupt changes in pitch.
- Implemented I²C communication with the LCD to display the generated frequency.

### Performance Optimization
One of the primary challenges was maintaining continuous audio output while performing ultrasonic measurements and updating the LCD on a single-core Arduino microcontroller.

Ultrasonic measurement functions and LCD communication introduced blocking operations that interfered with audio generation.

To improve performance, I:
- Reduced LCD update frequency to approximately 2 Hz.
- Refined sensor measurement and audio processing behavior.
- Implemented frequency smoothing to improve transitions between pitches.
- Integrated Mozzi audio processing with the sensor and display functionality.

These changes improved the responsiveness and continuity of the generated audio.

### Hardware Integration and Testing
- Assisted with integrating the ultrasonic sensors, LCD, and audio output circuitry.
- Worked with a PWM-based audio output system incorporating low-pass filtering and external amplification.
- Participated in iterative hardware and software debugging.
- Verified generated frequencies against an external pitch-checking tool.

## Hardware Components

| Component | Function |
|---|---|
| Arduino-compatible SparkFun RedBoard | Main microcontroller |
| Two ultrasonic distance sensors | Independent pitch and volume inputs |
| 16×2 I²C LCD | Real-time frequency display |
| Low-pass filter | Audio signal filtering |
| External amplifier | Audio signal amplification |
| 8Ω speaker | Sound output |

## Software and Tools

**Programming Language:** C++

**Development Platform:** Arduino IDE

**Audio Library:** Mozzi

**Communication Protocol:** I²C

**Testing:** Frequency verification, sensor measurements, and hardware debugging

## System Operation

1. The ultrasonic sensors measure the distances between the user's hands and the instrument.
2. The microcontroller maps sensor readings to pitch and volume parameters.
3. The Mozzi library generates the corresponding audio signal.
4. Software interpolation smooths transitions between successive frequency measurements.
5. The LCD displays the generated frequency in hertz.
6. The filtered and amplified audio signal is played through the speaker.

## Testing and Results

### Frequency Verification

The instrument was evaluated through 10 frequency verification trials spanning approximately 100–700 Hz.

Generated frequencies were compared against an external pitch-checking tool to evaluate the accuracy of the LCD frequency display.

### Sensor Performance

The ultrasonic sensors provided usable hand-distance measurements over approximately 0–60 cm, with readings generally stable within a few centimeters.

### Audio Performance

Software refinements reduced interruptions associated with sensor measurements and LCD updates, resulting in smoother pitch transitions and more continuous audio output.

## Project Images

*Photographs of the completed Theremin, hardware assembly, and LCD frequency display will be added here.*

## Firmware

The Arduino C++ firmware will be included in this repository.

The software integrates ultrasonic sensor measurements, frequency mapping, Mozzi audio generation, pitch smoothing, and LCD communication.

## Project Documentation

[View Final Engineering Report](Accessible-Theremin-Final-Report.pdf)

The report documents the system architecture, electrical design, software development, testing procedures, and final results.

## Project Attribution

This project was developed by a four-person engineering team.

My primary responsibility was embedded software development, including sensor processing, audio generation, LCD communication, performance optimization, and software testing. Hardware design and assembly were collaborative efforts, with another team member leading the electronics design.
