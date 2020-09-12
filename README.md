Arduino GLS29EE512 programmer
=============================
This is if you ever need to program *specifically* a GLS29EE512 flash EEPROM because maybe your
fancy universal programmer can't deal with different pin assignments.

I think this is the same chip as the SST29EE512, but I don't have any of those.

<a href="https://user-images.githubusercontent.com/4196901/92999474-1ed72a00-f4ef-11ea-9b98-88542afb2b27.jpg">
<img src="https://user-images.githubusercontent.com/4196901/92999474-1ed72a00-f4ef-11ea-9b98-88542afb2b27.jpg" alt="Arduino clone connected to an EEPROM by many wires" width="450">
</a>


Requirements
------------
For software, you'll need:

 * Arduino IDE
 * Python 3.7 or higher with [pyserial] and [tqdm]

For hardware, you'll need:
 * an Arduino Mega 2560*
 * some kind of adapter for your EEPROM
 * many patch wires

(*) Other Arduinos should work, but you'll probably need to reassign the ports at the top of
[GLS29EE512_programmer.ino] if you're not using ports A, C, G, and L.

[pyserial]: https://github.com/pyserial/pyserial
[tqdm]: https://github.com/tqdm/tqdm
[GLS29EE512_programmer.ino]: https://github.com/cknave/gls29ee512-programmer/blob/master/GLS29EE512_programmer.ino


Setting up the Arduino
----------------------
Open [GLS29EE512_programmer.ino] in the Arduino IDE and press the upload button.

Now connect the pins. See the [data sheet] for the pin assignments on the EEPROM.  The table below
includes the Arduino pin to port mappings for my 2560 clone, but you may need to scour the internet
for different models.

| AVR port | Arduino pins | GLS29EE512 pins |
| -------- | ------------ | --------------- |
| PORTA    | 29..22       | A7..A0          |
| PORTC    | 30..37       | A15..A8         |
| PORTL    | 42..49       | DQ7..DQ0        |
| PORTG    | 39, 40, 41   | WE#, CE#, OE#   |
| -        | 5V           | VDD             |
| -        | GND          | DSS             | 

On my board, getting power from the 5V and GND pins near the GPIO pins was more reliable than the
pins on the POWER section of the board.

You should now be able to open the serial monitor in the Arduino IDE and set the baud rate to
500,000.  The following will be printed if your EEPROM was detected successfully:

    Manufacturer: BF, product: 5D
    ready>

Make sure to close the serial monitor before trying to use the programmer script.

[data sheet]: https://www.greenliant.com/dotAsset/40945.pdf


Using the programmer script
---------------------------
Install the script requirements.  I recommend doing this in a virtual environment so you're not
trapped in Python dependency hell:

    pip install -r requirements.txt

If there is one serial device in `/dev/ttyACM*`, it will be used.  Otherwise, you'll need to set the
device with the `-d <device>` option.

To dump the EEPROM contents to output.bin:

    gls29ee512.py dump output.bin

To verify the EEPROM contents match input.bin:

    gls29ee512.py verify input.bin

To write input.bin to the EEPROM:

    gls29ee512.py write input.bin


Serial protocol reference
-------------------------
The arduino's serial port is set to 500,000 baud.  It uses a simple line-based protocol with `\n`
line terminators (though `\r\n` *should* be tolerated as well).

When the Arduino sends the `>` character, it is ready to accept commands.

When it sends the `#` character, it is ready to accept 128 bytes of data (or will output 128 bytes
of data in read mode).

Pages are 128 bytes, so valid page numbers are 0 through 511 inclusive for a 64 KiB EEPROM.

### Commands

    help

Print some help text

    hexdump <page>

Read a 128 byte page of data and print a hex dump. 

    read <page>

Read a 128 byte page of data.

After sending this command, wait for a `#` character and then read the next 128 bytes.

    verify <page>

Verify a 128 byte page of data.

After sending this command, wait for a `#` character and then send the expected 128 bytes of data.

Read the response up to the `>` character.  If it contains the string `OK`, then the page matches.

    write <page>

Write a 128 byte page of data.

After sending this command, wait for a `#` character and then send the 128 bytes of data to write.

Read the response up to the `>` character.  If it contains the string `OK`, then the page was
successfully written.
