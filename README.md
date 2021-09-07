## ICSP Codec
# Introduction
This is a implementation of video codec based on C/C++. <br />
This codec is ICSP codec because I implemented this codec when I was working in the ICSP lab(http://icsp.hanyang.ac.kr).<br />
ICSP Codec has been implemented based on basic video compression theories.
The characteristics of the ICSP codec are as follows.
- 16x16 macro block unit process(One 16x16 macro block is divided by four 8x8 blocks in each compression process)
- Intra Prediction / Inter Prediction
- All Intra Prediction Mode
- DCT Transform / Quantization
- Entropy Coding
- Only Supporting YUV420
- Only Supporting CIF(352x288)
- multi-thread encoding

# Build and execute
<pre>
<code>
> cd build/Debug(or Release)
> ./build.sh
> make
> ./ICSPCodec [option] [values]
</code>
</pre>

# Encoding options  
- -i : input yuv sequence
- -w : width
- -h : height
- -n : the number of frames(default is 1)
- -q : QP of DC and AC (16, 8, or 1)
- -h : help message
- --help : help message
- --qpdc : QP of DC (16, 8, or 1)
- --qpac : QP of AC (16, 8, or 1)
- --intraPeriod: period of intra frame(0: All intra)
- --EnMultiThread: enable multi threading mode, also the number of thread(0~4, 0 is disable)

# ICSP Codec Encoding Process
![Encoding Proces](https://user-images.githubusercontent.com/36951642/57179748-f59a9880-6ebb-11e9-9c6d-5857f45d5545.PNG)

Fig. 1 ICSP codec structure

# Algorithm
### Intra Prediction
ICSP codec supports three-direction prediction mode in intra prediction. Vectical, Horizontal, DC mode.

![Intra prediction](https://user-images.githubusercontent.com/36951642/57229516-41cb1180-7051-11e9-83a0-da9e953b989a.png)
Fig. 2 Intra prediction mode

The green square is a reconstructed reference pixel around the current block. Intra prediction uses a reference pixel corresponding to each direction mode to generate a prediction block. DC mode generate prediction block using mean value of vertical and horizontal reference pixels. If reference pixels does not exist by some reason(first block, first row blocks, first colum blocks...), then it is replaced by integer value 127. You can set a period of a intra prediction frame in one sequcece by initializing 'intraPeriod' as a integer value. For example, if you initialize 'intraPeriod' as 10 and one sequence consists of 300 frames, then every 10 frames are encoded as intra prediction mode and other frames are encoded as inter prediction.

#### All Intra Prediction Mode
ICSP Codec supports special mode, All Intra Prediction Mode. All Intra Prediction Mode encodes all of frames using intra prediction mode. You can operate ICSP Codec as All Intra Prediction Mode by initializing 'intraPeriod' veriable as ALL_INTRA(2; need to be fixed) macro value.
### Inter Prediction
ICSP Codec supports only backword prediction. That is, inter prediction blocks are generated by using only previous frame(accurately, the previous frame is a reconstructed frame). 

![image](https://user-images.githubusercontent.com/36951642/57631044-b2d27200-75d9-11e9-8997-8c8d64738c97.png)

Fig. 3 example of inter prediction

Basically, inter prediction process is to find the location of the most similar block with current block in previous frame(fn-1). Also, current blocks are in current frame(fn). in the figure above, red block is a current block in current frame. Green block is the one closest to current block among neighbor blocks around red block in previous frame. ICSP Codec uses search method as 'spiral search'.
Spiral search in ICSP codec means that search the block by round and round the current block position like sprial in pixel units.

![image](https://user-images.githubusercontent.com/36951642/57704034-1fae4080-769c-11e9-91ef-c10880442f18.png)

Fig. 4 spiral search

I attached the above picture for conceptual understanding about spiral search. I hope there is no misunderstanding. ICSP codec moves search block in pixel units and finds a similar block which has minimum SAD(Sum of Absolute Difference) between a current block and a seach block.

Up to this point, it was a conceptual explanation of the inter prediction. The inter prediction consists of motion estimation and motion compensation. I will briefly explain these two processes.

#### Motion Estimation
Simply speaking, motion estimation is to generate a motion vector that represents the distance between a current block position and a similar block which has the smallest SAD.

![image](https://user-images.githubusercontent.com/36951642/57705527-12468580-769f-11e9-9312-a06ecd36a4b4.png)

Fig. 5 example of motion estimation

#### Motion Compensation
In ICSP Codec, motion compensation is to make prediction blocks using motion vectors and to make residual blocks(current block - prediction block) in results. Residual blocks are used to other encode/decode process like DCT, Quantization, entropy coding process.

### DCT
Discrete Cosine Transform(DCT) decomposes a signal for time or spatial domain into frequency components such as Fourier transform.
That is, DCT transforms the image of the spatial domain into the frequency domain. When a 8x8 block in spatial domain is transformed by DCT, it is decomposed a DC(direct current) component having no frequency and sixty-three AC(alternating current) components having a frequency.
Since most image are composed of a large number of low frequency components and a few high frequency components, when DCT is applied to image, most of energy is concentrated in DC and just a few energy are widely spread to AC components having high frequency. This phenomenon is called 'Energy compaction' which is very widely used in the video compression field. <br/>
ICSP codec supports 8x8 block unit for DCT. A 8x8 image block is decomposed by one DC(direct current) component which is having no frequency and sixty-three AC(alternating current) components having a relatively high frequency. 

![image](https://user-images.githubusercontent.com/36951642/58094372-7cb17580-7c0b-11e9-8887-5e9c36321c7a.png)

Fig. 6 example of DCT

When the image block of the spatial domain is transformed into the frequency domain through the DCT. Most of energy us concentrated on the DC component and the low frequency AC component, and the remaining energy is widely distributed on the high frequency AC components.<br/>
the above figure is intended to explain DCT, so it is different from the exact DCT result. In fact, the image block used for DCT in the ICSP codec is a residual block and DCT result is stored as a floating point(double).

### Quantization
Quantization is a process of dividing coefficients in a transformed block by a constant(quantization step, QStep). Through quantization, most of the AC Components which have a high frequency component have 0 value or a very small value. The more AC components with very small values, the less bits are allocated in entropy coding process, resulting in a compression efficiency increase. However, this process directly affects to image quality. The larger the QStep, the larger the loss of image quality.<br/>
ICSP codec supports independent AC and DC component quantization(QStepAC, QStepDC). Also, ICSP codec provides 1, 8 and 16 QStep.

![image](https://user-images.githubusercontent.com/36951642/58096758-bfc21780-7c10-11e9-8101-c59793486858.png)

Fig. 7 example of quatization

### ZigZag Scanning(Reordering)
Zig-zag scanning arranges elements of a quantized block in certain order. The order is not just sequential order, but diagonal order like zig-zag order. please refer below figure.

![image](https://user-images.githubusercontent.com/36951642/58099869-7a551880-7c17-11e9-9c7c-f421f1591de5.png)

Fig. 8 zig-zag scanning

By zig-zag reordering, a DC component and AC components with low frequency are arranged on front side and AC components with high frequency are arranged on rear side. If QStep is large(16 or bigger than 16), most elements of high frequency have 0 or very small values, so a number of 0 values are arranged on rear side. <br/>
In above figure case, the result of zig-zag reordering is like 61,13,12,0,11,7,0,6,0,...,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0... Most of values on rear side are 0. In ICSP codec, if all of AC components are 0, some unique process is applied into bitstream for compression efficiency by using AC flag. This technique will be explained in detal in BitSteam Syntax part.

### Entropy Coding
In Entropy coding, The list of numbers from zig-zag reordering is converted into a bitstream. Each numbers in the list are converted into bundle of bits. The amount of bits allocated depends on the frequency of occurence of the number. In above case, a few bits are allocated to 0 because 0 is the highest number of occurrences. Conversely, many bits are allocated to other numbers such as 63, 13, 12.... If the number of 0 values are larger in the list, the size of bitstream will become smaller and smaller. ICSP codec has already determined codeword according to certain range of integers, I didn't implement huffman coding by myself. I just convert a number into determined code word according to the range to which the number has belonged.

### Bitstream Syntax
#### Stream Header
The Header of ICSP bitsream has fixed structure like below figure. The size of the header is 14 bytes.

![image](https://user-images.githubusercontent.com/36951642/58371106-76900180-7f49-11e9-9c61-38dfb385dd83.png)

Fig. 9 stream header of ICSP codec

#### Stream Body
##### Intra Prediction Frame Body

![image](https://user-images.githubusercontent.com/36951642/58421558-35275f80-80cb-11e9-9059-387dac665c3a.png)

Fig. 10 stream body for intra frame

##### Inter Prediction Frame Body

![image](https://user-images.githubusercontent.com/36951642/58421530-1e810880-80cb-11e9-9841-d00d27584c01.png)

Fig. 11 stream body for inter frame

## Advanced 

### Vectorization Mode(SIMD, AVX Intrinsics)
ICSP Codec have been basically implemented by pixel unit process(Scalar process). 
Recently, I converted this pixel unit process into vectorization process using AVX Intrinsics fucntions.
Since I didn't familiar with SIMD yet, There may be some unskillful logic and code. 
However, This conversion significantly shortened the computational complexity in similar quality.
![image](https://user-images.githubusercontent.com/36951642/67497244-ab4ee300-f6b8-11e9-8687-01cb371a0d10.png)

Fig. 12 simple example for vectorization structure (using AVX)

Currently, Vectorization Mode is temporarily not supported. I will be working on improvements for stability and scalability. If you want to see the previous source, please refer to the 53deac7 previous commit.

### Multi-thread Mode
ICSP Codec only support closed gop structure(P frame can only reference previous frame that is belong to same gop). 
YUV sequence is divided into gop units. Gop becomes a job and is stored in the queue, and the encoding thread takes each job out of the queue and proceeds with encoding.
Fig1 is encoding thread.

<img width="881" alt="image" src="https://user-images.githubusercontent.com/36951642/130348438-3f1ba364-c6a2-43a2-970f-4f2c54b014df.png">

Fig. 13 multi-thread structure of ICSP codec

### Computational Complexity Comparison 
#### All Intra Prediction Mode
| Sequences | Scalar Encoding Time(Sec) | AVX Encoding Time(sec)| Encoding Time Reduction Rate(%)|
|---|:---:|:---:|:---:|
|akiyo|13.65|12.12|89%|
|children|16.9|12.91|76%|
|coastguard|13.98|12.36|88%|
|container|14.84|11.38|77%|
|football|5.05|4.04|80%|
|foreman|17.21|12.42|72%|
|hall monitor|14.31|12.83|90%|
|mobile|15.3|12.01|79%|
|mother_daughter|13.72|12.01|88%|
|news|14.26|11.41|80%|
|stefan|16.35|13.06|80%|
|table|13.62|11.33|83%|
|**Average**|x|x|**82%**|


#### Inter/Intra Hybrid Prediction(intra period : 10)
| Sequences | Encoding Time(Sec) | AVX Encoding Time(sec)| Encoding Time Reduction Rate(%)|
|---|:---:|:---:|:---:|
|akiyo|30.52|13.71|45%|
|children|25.9|11.27|44%|
|coastguard|30.71|12.51|41%|
|container|28.58|13.31|47%|
|football|9.59|4.4|46%|
|foreman|29.33|11.94|41%|
|hall monitor|30.53|12.31|40%|
|mobile|29.08|12.36|42%|
|mother_daughter|30.95|12.36|40%|
|news|26.34|11.43|43%|
|stefan|29.04|13.09|45%|
|table|28.51|12.81|45%|
|**Average**|x|x|**43%**|

### Contents
| Folder | description |
|---|---|
|data| test sequences |
| source | C++ source code |
