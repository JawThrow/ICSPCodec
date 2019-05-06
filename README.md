## ICSP Codec
### Introduction
This is a implementation of video codec based on C/C++. <br />
This codec is ICSP codec because I implemented this when I was working in the ICSP lab(https://icsp.hanyang.ac.kr).
ICSP Codec has been implemented based on basic video compression theories.<br />
The characteristics of the ICSP codec are as follows.
- 16x16 macro block unit process(One 16x16 macro block is divided by four 8x8 blocks in each compression process)
- Intra Prediction / Inter Prediction
- All Intra Prediction Mode
- DCT Transformation / Quantization
- Entropy Coding
- AVX Intrinsics Parellel Processing

If you have any kinds of feedback and question, please contact me.

### ICSP Codec Encoding Process
![Encoding Proces](https://user-images.githubusercontent.com/36951642/57179748-f59a9880-6ebb-11e9-9c6d-5857f45d5545.PNG)

### Intra Prediction
ICSP codec supports three-direction prediction mode in intra prediction. Vectical, Horizontal, DC mode.

![Intra prediction](https://user-images.githubusercontent.com/36951642/57229516-41cb1180-7051-11e9-83a0-da9e953b989a.png)

The green square is a reconstructed reference pixel around the current block. Intra prediction uses a reference pixel corresponding to each direction mode to generate a prediction block. DC mode generate prediction block using mean value of vertical and horizontal reference pixels. If reference pixels does not exist by some reason(first block, first row blocks, first colum blocks...), then it is replaced by integer value 127.

### Inter Prediction

### All Intra Prediction Mode

### DCT Transformation & Quantization

### Entropy coding

### AVX Intrinsics mode

### Contents
| Folder | description |
|---|---|
|data| test sequences |
| source | C++ source code |
