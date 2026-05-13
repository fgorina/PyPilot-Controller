# Pypilot controller

This is a M5Though controller for PyPilot.

It makes a TCP connection to the PyPilot at port 23322 and opens a http server service to configure :
  - Network
  - Password

There are 2  characteristics in a BLE Server so we will be able to control the device from an AppleWatch or other BLE device.

  - Command : Writable, you may send commands
  - State : Readable, Notify sends state

Will translate commands receive from Command to Pypilot commands and send in state 
pypilot data as :
  - Heading
  - Command
  - Rudder Angle
  - PyPilot Mode
  - Engaged / Not Engaged

  Allowing to write BLE controllers that don't need a TCP connection to PyPilot

## Use and configuration
The old characteristics for configuration must be removed and goes to the http server. The service remains for BLE Control

  Service f85015df-6af5-4ee3-a8cb-a8f7250d4466

/*
First we must give the controller the ssid and password of the network writing :

  - Characteristic bea80929-aa42-4641-a0c2-8f08b70e0aaa ssid
  - Characteristic 22643f77-dcfd-4e01-9b9f-bb63692a215f password

  */

  It should lookup for pypilot with mDNS. If changing ip touch the screen when powering up and will clear old host and lookup for a new one.

When connects shows a screen with the 4 modes (Compass, gps, wind, true wind) and a center rudder button (MenuScreen). They may be selected just by tapping on them.

In rudder you manage the rudder directly and have a button for centering it.

In other modes it shows the Heading in the center, an editable desired heading at top, and the buttons for +/- 1º, 10º.

A LongPress in the +/- 10º "Arms the tacking". A tap in the same button starts it. In other places disarms the tapping. When armed the button changes to orange and show a turning arrow.
A tap while tacking stops the tack. When tacking end button is restores to default state.


Of course only acceptable modes will be accepted by pypilot depending it's hardware and conections.


## BLE Gateway

The M5Dial also has characteristics 

  - Characteristic 02804fff-8c38-485f-964a-474dc4f179b2 Command (writable)
  - Characteristic de7f5161-6f48-4c9a-aacc-6079082e6cc7 State (readable, notify)

Usually an application subscribes to State and sends commands to command.

### State

Messages received from State are composed by a first letter (type) and the rest are parameters. They are sent by us

Types of message are

  - E : Engaged
  - D : Disengaged
  - H<degrees> : Heading
  - R<degrees> : Rudder Angle
  - C<degrees> : Command
  - M<int>  : Mode where <int> is:
     - 0 -> Compass
    - 1 -> Gps
    - 2 -> Wind
    - 3 -> True Wind
    - 4 -> Manual / Rudder control
  - T<int> : Tack State where <int> is:
    - 0 -> none, no tacking
    - 1 -> begin
    - 2 -> waiting
    - 3 -> tacking

  - U<int> : Tack Direction where <int> is:
    - -1 -> Port
    - 0 -> None
    - 1 -> Starboard

### Commands

Commands are sent with a string where the first letter is the command and the rest parameters

  - E -> Engage
  - D -> Disengage
  - C<angle> -> Set Command to <angle>
  - M<int> -> Set Mode. See modes before
  - TP -> Tack port
  - TS -> Tack Starboard
  - X -> Cancel Tack
  - RP -> Rudder to Port (value 1.0)
  - RS -> Rudder to Starboard (value 1.0)
  - Z<angle> -> Set rudder angle to <angle>

