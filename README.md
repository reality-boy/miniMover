## miniMover

Code to control XYZ 3D printers that use the version 3 serial protocol.  
These include the Nano, Mini W, miniMaker and Jr. line of printers.

You can read up on the development progress in this forum.
http://www.soliforum.com/topic/17028/print-gcode-files-to-minimaker/

This is inspired by the threedub programs listed below but it is based 
on a fresh reverse engineering of the serial protocol.

This uses a modified version of the tiny-AES project by kkoke.
https://github.com/kokke/tiny-AES-c

This comes in two flavors, a win32 GUI and a command line utility.

![My image](http://soliforum.com/i/?8IOJXEv.png)

Here is a summary of the commands that the command line can take.

```
usage: miniMover <args>
  -? - print help message
  -a+ - enable auto level
  -a- - disable auto level
  -b+ - enable buzzer
  -b- - disable buzzer
  -cl - clean nozzel
  -ca - calibrate bed
  -c file - convert file
  -f file - upload firmware, experimental!
  -h - home printer
  -l - load fillament
  -o num - increment/decrement z offset by num
  -po num - set serial port number, -1 auto detects port
  -p file - print file
  -s - print status
  -u - unload fillament
  -x num - jog x axis by num, or 10 if num not provided
  -y num - jog y axis by num, or 10 if num not provided
  -z num - jog z axis by num, or 10 if num not provided
  file - print file if .gcode, otherwise convert to gcode if .3w
```

#Here are some related projects:

https://gitlab.com/anthem/py-threedub

https://gist.github.com/companje/b700748a49f4af73d57011c644f5a778

http://www.soliforum.com/topic/15639/jr-cura-230-threedub/

https://github.com/mwm/xyzify

https://www.thingiverse.com/thing:1915076
