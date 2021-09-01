/****************************************************************************

    DRC: Digital Room Correction
    Copyright (C) 2002-2004 Denis Sbragion, 2004 Edward Wildgoose

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

		You can contact the author on Internet at the following address:

				d.sbragion@infotecna.it

		This program uses the parsecfg library from Yuuki  NINOMIYA.  De�
		tails  on  this  library  can be found in the parsecfg.c and par�
		secfg.h files.  Many thanks to Yuuki NINOMIYA for this useful li�
		brary.

		This program uses  also the FFT  routines from  Takuya Ooura and
		the GNU Scientific  Library (GSL).  Many thanks  to Takuya Ooura
		and the GSL developers for these efficient routines.

****************************************************************************/

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "fftsg_h.h"
#include "RtAudio.h"
#include <cstring>
#include <unistd.h>

using namespace std;

/* Decommentare per abilitare la compilazione in doppia precisione */
/* Uncomment to enable double precision computation */
/* #define UseDouble */


#ifdef UseDouble
	/* Tipo floating point usato per le elaborazioni */
	#define DLReal double
#else
	/* Tipo floating point usato per le elaborazioni */
	#define DLReal float
#endif

#ifndef M_PI
	#define M_PI ((DLReal) 3.14159265358979323846264338327950288)
#endif

#ifndef M_2PI
	#define M_2PI ((DLReal) 6.28318530717958647692528676655900576)
#endif

/* Stuff for the record and playback code */
typedef float  MY_TYPE;
#define FORMAT RTAUDIO_FLOAT32
#define SCALE  1.0

#ifdef WIN32
//    #define BUFFER_SIZE 2048
    #define BUFFER_SIZE 1024
#else
    #define BUFFER_SIZE 1024
#endif

//#define SAMPLE_BUFFERS 8
#define SAMPLE_BUFFERS 4
#define LATENCY 2
#define BASE_RATE 0.005
#define TIME   1.0

//#define KEEP_TEMP_FILES 1 // For unix, keeps the temp files


/******************************************************************/


/* Open a file and set buffering on the file */
inline FILE* FOpen(const char* FName,const char* mode)
{
    FILE* fp=fopen(FName,mode);
    if ( !fp ) {
        fprintf(stderr, "Error opening file: %s \n", FName);
        exit(-1);
    }
    setvbuf(fp,NULL,_IOFBF,64*1024);
    return fp;
}


/* Create a temporary filename in temporary directory with the
 * prefix "prefix". The actual destination directory may vary
 * depending on the state of the TMP environment variable and
 * the global variable P_tmpdir.    */
FILE* getTempFile(char *prefix)
{
    FILE* fp;

#ifdef WIN32

/*
    char *result = _tempnam( "c:\\tmp", prefix );
    if( result == NULL ) {
        fprintf( stderr, "Problem creating temp file name\n" );
        return NULL;
    } else {
        if( (fp = fopen( result, "w+b" )) != NULL ) {
            setvbuf(fp,NULL,_IOFBF,64*1024);
            free (result);
            return fp;
        } else {
            fprintf( stderr, "Problem opening temp file\n" );
            free (result);
            return NULL;
        }
    }
*/
    if ( (fp = tmpfile()) == NULL) {
        cerr << "Problem creating temp file name\n";
        return NULL;
    } else {
        setvbuf(fp,NULL,_IOFBF,64*1024);
        return fp;
    }

#else
    if (strlen(prefix)>10) {
        fprintf(stderr, "temp file name prefix too large in get_temp_name");
        exit(1);
    }
    char tempn[50] = "/tmp/";
    strcat(tempn, prefix);
    strcat(tempn, "XXXXXX");

    int fd = mkstemp( tempn );
    if (fd != -1) {
        if( (fp = fdopen(fd, "r+b") ) != NULL ) {
            setvbuf(fp,NULL,_IOFBF,64*1024);
#ifndef KEEP_TEMP_FILES
            unlink(tempn);
#endif
            return fp;
        } else {
            fprintf( stderr, "Problem opening temp file\n" );
            close(fd);
            return NULL;
        }
    } else {
        fprintf( stderr, "Problem creating temp file\n" );
        return NULL;
    }

#endif
}


/****************************************************************************
* Read from FILE into buffer
* Buffer is assumed to be interleaved samples of length "buffer_size"
* and has a number of channels given by "channels"
* The mono input stream is written to channel number "channel"
* NOTE: Buffer left zero-ed if we reach the end of file!
* RETURN: Number of samples read from file
*****************************************************************************/
size_t fill_sample_buffer(MY_TYPE *buffer,int buffer_size,int channels,int channel1, int channel2, FILE *fp)
{
    size_t frames_read;
    // Read some bytes from the file
    float *buf_out = new float[buffer_size]; // for writing back to output file
    frames_read = fread(buf_out, sizeof(MY_TYPE), buffer_size, fp);
    // Zero the buffer
    memset(buffer, 0, channels*sizeof(MY_TYPE)*buffer_size);

    // Write bytes into audio buffer
    for (unsigned int i=0; i<frames_read; i++) {
        buffer[i*channels+channel1] = buf_out[i];
        if (channel2 >= 0)
            buffer[i*channels+channel2] = buf_out[i];
    }

    delete buf_out;
    return frames_read;
}


/****************************************************************************
* Read sample and play through card, record corresponding samples and write back to disk
* Buffer is assumed to be interleaved samples of length "buffer_size"
* and has a number of channels given by "channels"
* The mono input stream is written to channel number "channel"
* NOTE: Buffer left zero-ed if we reach the end of file!
*****************************************************************************/

void playRecord(FILE *fpIn, FILE *fpOut, FILE *fpLoop, int fs, int deviceIn, int deviceOut, int chanIn, int chanOut, int loopIn, int loopOut)
{
    int buffer_size = BUFFER_SIZE;
    RtAudio *audio;
    MY_TYPE *buffer;
    int buffs_to_drain = 0;
    int frameNum = 0;
    float peakRecLevel = 0, peakLoopLevel = 0;
    int peakRecPos = 0;
    bool abort = false;

    RtAudioDeviceInfo infoIn, infoOut;
    try {
        audio = new RtAudio();
        if (deviceIn !=0) infoIn = audio->getDeviceInfo(deviceIn);
        if (deviceOut !=0) infoOut = audio->getDeviceInfo(deviceOut);
        delete audio;
    }
    catch (RtError &error) {
        error.printMessage();
        exit(EXIT_FAILURE);
    }

    // Open all the chans available, or just enough?
    int chansIn, chansOut;
    if ( (deviceOut == deviceIn) && (deviceOut != 0) ) {
        chansIn = chansOut = infoIn.duplexChannels;
    } else {
        if (deviceIn != 0)
            chansIn = infoIn.inputChannels;
        else
            chansIn = max(2, chanIn + 1);
        if (deviceOut != 0)
            chansOut = infoOut.outputChannels;
        else
            chansOut = max(2, chanOut + 1);
    }

    // Check that the requested channels are rational
    if (chanIn >= chansIn || chanOut >= chansOut) {
        cerr << "Error: Invalid chanIn or chanOut requested\n";
        cerr << "Input channel: " << chanIn << " requested.  " << chansIn << " channels available.\n";
        cerr << "Output channel: " << chanOut << " requested.  " << chansOut << " channels available.\n";
        exit (-1);
    }


    // OK now we start doing something

    // Open the realtime output device
    try {
        cout << "\nOpening Audio Devices: " << "device out: " << deviceOut << " chans out: "
	     << chansOut << " deviceIn: "<< deviceIn << " chans In: " << chansIn << "\n";
        audio = new RtAudio(deviceOut, chansOut, deviceIn, chansIn,
            FORMAT, fs, &buffer_size, SAMPLE_BUFFERS, RtAudio::UNSPECIFIED);
    }
    catch (RtError &error) {
        error.printMessage();
        exit(EXIT_FAILURE);
    }

    // Get ready to go
    float *buf_in = new float[buffer_size]; // for reading in from temp file
    rewind(fpOut);

    try {
        buffer = (MY_TYPE *) audio->getStreamBuffer();
        audio->startStream();
    }
        catch (RtError &error) {
        error.printMessage();
        goto cleanup;
    }

    cerr << "Playing & Recording Audio ... fragment_size = " << buffer_size << endl;

    size_t frames_read;
    // Duplex playback and record from card
    do {

        frames_read = fill_sample_buffer(buffer, buffer_size, chansOut, chanOut, loopOut, fpOut);

        if (!feof(fpOut) && (buffs_to_drain < LATENCY) ) {
            // Loading sample buffer
//            try { audio->primeOutputBuffer(); } // Write audio only
//            catch (RtError &error) { error.printMessage(); }

            buffs_to_drain++;

        } else {
            // Ticking over normally
            try { audio->tickStream(); } // Write audio, read recorded
            catch (RtError &error) { error.printMessage(); }

            // Read recorded buffer, extract mono microphone channel
            for (int i=0; i<buffer_size; i++) {
                buf_in[i] = buffer[i*chansIn+chanIn];
                // Check for clipping
                if (fabs(buf_in[i]) > 0.98)
                {
                    cerr << "Warning: Probable clipping occurred on microphone input. Peak "
                        << buf_in[i] << " ( " << 20*log10((double)fabs(buf_in[i])) << " ) db)\n";
                    abort = true;
                    exit(-1);
                }
                if (fabs(buf_in[i]) > peakRecLevel) {
                    peakRecLevel = fabs(buf_in[i]);
                    peakRecPos = frameNum;
                }
                frameNum++;
            }
            if (fwrite(buf_in, sizeof(float), buffer_size, fpIn) != (size_t)buffer_size) {
                cerr << "Error writing recorded sweep file out in playRecord()\n";
                exit (-1);
            }

            // Read recorded buffer, extract reference loopback channel
            if (loopOut >= 0) {
                for (int i=0; i<buffer_size; i++) {
                    buf_in[i] = buffer[i*chansIn+loopIn];
                    // Check for clipping
                    if (fabs(buf_in[i]) > 0.98)
                    {
                        cerr << "Warning: Probable clipping occurred on loopback input. Peak "
                            << buf_in[i] << " ( " << 20*log10((double)fabs(buf_in[i])) << " ) db)\n";
                        abort = true;
                        exit(-1);
                    }
                    if (fabs(buf_in[i]) > peakLoopLevel)
                        peakRecLevel = fabs(buf_in[i]);
                }

                if (fwrite(buf_in, sizeof(float), buffer_size, fpLoop) != (size_t)buffer_size) {
                    cerr << "Error writing recorded reference loopback file out in playRecord()\n";
                    exit (-1);
                }
            }

            if (feof(fpOut) && (buffs_to_drain > 0) )
                buffs_to_drain--;
        }

    } while ( (!feof(fpOut) || buffs_to_drain > 0) && !ferror(fpOut) && !abort );


    // Stop the stream.
    try {
        audio->stopStream();
    }
    catch (RtError &error) {
        error.printMessage();
    }

    delete buf_in;

    // Lets show some statistics and warnings if necessary
    cout << "Finished Recording\n";

    cout << "Peak Mic level: "
        << peakRecLevel << " ( " << 20*log10(peakRecLevel) << " ) db) - Position: " << peakRecPos << "\n";
    if (peakRecLevel > 0.98)
        cerr << "Warning: Clipping was detected on the microphone input.  Reduce the preamp gain, or reduce the playback levels\n";
    else if (peakRecLevel < 0.03125)
        cerr << "Warning: Microphone input levels are quite low.  Increase the preamp gain, or increase the playback levels\n";

    if (loopOut >= 0) {
        cout << "Peak Loop level: "
            << peakLoopLevel << " ( " << 20*log10(peakLoopLevel) << " ) db)\n";
        if (peakLoopLevel > 0.98)
            cerr << "Warning: Clipping was detected on the loop input.  Reduce the preamp gain, or reduce the playback levels\n";
        else if (peakLoopLevel < 0.03125)
            cerr << "Warning: Loop input levels are quite low.  Increase the preamp gain, or increase the playback levels\n";
    }

    cout << "\n";


cleanup:
    audio->closeStream();
    delete audio;
}


int showDeviceInfo()
{
    RtAudio *audio;
    RtAudioDeviceInfo info;
    try {
        audio = new RtAudio();
    }
    catch (RtError &error) {
        error.printMessage();
        exit(EXIT_FAILURE);
    }

    int devices = audio->getDeviceCount();
    cerr << "\nFound " << devices << " device(s) ...\n";

    for (int i=1; i<=devices; i++) {
        try {
        info = audio->getDeviceInfo(i);
        }
        catch (RtError &error) {
        error.printMessage();
        break;
        }

        cerr << "\nDevice: #" << i << '\n';
        cerr << "Device Name = " << info.name << '\n';
        if (info.probed == false)
        cerr << "Probe Status = Unsuccessful\n";
        else {
        cerr << "Probe Status = Successful\n";
        cerr << "Output Channels = " << info.outputChannels << '\n';
        cerr << "Input Channels = " << info.inputChannels << '\n';
        cerr << "Duplex Channels = " << info.duplexChannels << '\n';
        if (info.isDefault) cerr << "This is the default device.\n";
        else cerr << "This is NOT the default device.\n";
        if ( info.nativeFormats == 0 )
            cerr << "No natively supported data formats(?)!";
        else {
            cerr << "Natively supported data formats:\n";
            if ( info.nativeFormats & RTAUDIO_SINT8 )
            cerr << "  8-bit int\n";
            if ( info.nativeFormats & RTAUDIO_SINT16 )
            cerr << "  16-bit int\n";
            if ( info.nativeFormats & RTAUDIO_SINT24 )
            cerr << "  24-bit int\n";
            if ( info.nativeFormats & RTAUDIO_SINT32 )
            cerr << "  32-bit int\n";
            if ( info.nativeFormats & RTAUDIO_FLOAT32 )
            cerr << "  32-bit float\n";
            if ( info.nativeFormats & RTAUDIO_FLOAT64 )
            cerr << "  64-bit float\n";
        }
        if ( info.sampleRates.size() < 1 )
            cerr << "No supported sample rates found!";
        else {
            cerr << "Supported sample rates = ";
            for (unsigned int j=0; j<info.sampleRates.size(); j++)
            cerr << info.sampleRates[j] << " ";
        }
        cerr << endl;
        }
    }
    cerr << endl;

    delete audio;
    return 0;
}


/***********************************************************************************
* Now the code to generate the sweeps
* We save to a temp file to give small machines
* with very little memory a stab at this
************************************************************************************/
int generateSweep(FILE *sweepFile, FILE* inverseFile,
              	int rate, int hzStart, int hzEnd, int duration, float silence)
{
	/* Input parameters */
	DLReal Rate = (DLReal)rate;
	DLReal Amplitude = (DLReal)0.5;
	DLReal HzStart = (DLReal)hzStart;
	DLReal HzEnd = (DLReal)hzEnd;
	DLReal Duration = (DLReal)duration;
	DLReal Silence = silence;
	DLReal LeadIn = (DLReal)0.05;
	DLReal LeadOut = (DLReal)0.005;

	/* Generation parameters */

	/* Base sweep generation */
	int SweepLen;
	int SilenceLen;
	DLReal W1;
	DLReal W2;
	DLReal Ratio;
	DLReal Sample;
	DLReal S1;
	DLReal S2;
	DLReal DecayTime;
	DLReal Decay;
	DLReal RMSV;
	DLReal RMS;
	int I;
	int J;
	float FS;

	/* Lead in and lead out Blackman windowing */
	int LeadInLen;
	DLReal WC1In;
	DLReal WC2In;
	int LeadOutLen;
	DLReal WC1Out;
	DLReal WC2Out;
	DLReal WC;

	/* Output file */
	FILE * OF;

	/* Computes internal generation values */
	cout << "Sweep generation setup.\n";

	/* Base sweep generation */
	SweepLen = (int) (Rate * Duration);
	SilenceLen = (int) (Rate * Silence);
	W1 = (DLReal) (HzStart * M_2PI);
	W2 = (DLReal) (HzEnd * M_2PI);
	Ratio = (DLReal) log(W2 / W1);
	S1 = (DLReal) ((W1 * Duration) / Ratio);
	S2 = (DLReal) (Ratio / SweepLen);
	DecayTime = (DLReal) (SweepLen * log(2.0) / Ratio);

	/* Lead in and lead out Blackman windowing */
    LeadInLen = (int) (LeadIn * SweepLen);
    WC1In = (DLReal) M_PI / (LeadInLen - 1);
    WC2In = (DLReal) M_2PI / (LeadInLen - 1);
    LeadOutLen = (int) (LeadOut * SweepLen);
    WC1Out = (DLReal) M_PI / (LeadOutLen - 1);
    WC2Out = (DLReal) M_2PI / (LeadOutLen - 1);

    /* Report generation parameters */
    printf("\nSweep length: %d samples\n",SweepLen);
    printf("Silence length: %d samples\n",SilenceLen);
    printf("Total sweep length: %d samples\n",2 * SilenceLen + SweepLen);
    printf("Total sweep file size: %d bytes\n",sizeof(float) * (2 * SilenceLen + SweepLen));
    printf("Total inverse length: %d samples\n",SweepLen);
    printf("Total inverse file size: %d bytes\n\n",sizeof(float) * SweepLen);
    fflush(stdout);

	/* Open the sweep file */
    OF = sweepFile;

	/* Generates the sweep file */
	cout << "Generating the sweep file...\n";

	/* Initial silence */
	FS = (DLReal) 0.0;
	for (I = 0;I < SilenceLen;I++)
		fwrite(&FS,sizeof(float),1,OF);

	/* Initial lead in */
	for (I = 0;I < LeadInLen;I++)
		{
			Sample = (DLReal) sin(S1 * (exp(I * S2) - 1.0));
			WC = (DLReal) (0.42 - 0.5 * cos(WC1In * I) + 0.08 * cos(WC2In * I));
			FS = (float) (Sample * WC * Amplitude);
			fwrite(&FS,sizeof(float),1,OF);
		}

	/* Full sweep */
	for (I = LeadInLen;I < SweepLen - LeadOutLen;I++)
		{
			Sample = (DLReal) sin(S1 * (exp(I * S2) - 1.0));
			FS = (float) (Sample * Amplitude);
			fwrite(&FS,sizeof(float),1,OF);
		}

	/* Final lead out */
	for (I = SweepLen - LeadOutLen,J = LeadOutLen;I < SweepLen;I++,J--)
		{
			Sample = (DLReal) sin(S1 * (exp(I * S2) - 1.0));
			WC = (DLReal) (0.42 - 0.5 * cos(WC1Out * J) + 0.08 * cos(WC2Out * J));
			FS = (float) (Sample * WC * Amplitude);
			fwrite(&FS,sizeof(float),1,OF);
		}

	/* Final silence */
	FS = (DLReal) 0.0;
	for (I = 0;I < SilenceLen;I++)
		fwrite(&FS,sizeof(float),1,OF);

	/* Close the sweep file */
	cout << "Sweep file generated.\n";

	/* Computes the inverse normalization factor */
	cout << "Inverse normalization computation...";
	RMS = 0;

	/* Final lead out */
	for (I = 0,J = SweepLen;I < LeadOutLen;I++,J--)
		{
			Decay = (DLReal) pow((DLReal)0.5,I / DecayTime);
			Sample = (DLReal) sin(S1 * (exp(J * S2) - 1.0));
			WC = (DLReal) (0.42 - 0.5 * cos(WC1Out * I) + 0.08 * cos(WC2Out * I));
			RMSV = (DLReal) (Sample * WC * Decay);
			RMS += RMSV * RMSV;
		}

	/* Full sweep */
	for (I = LeadOutLen,J = SweepLen - LeadOutLen;I < SweepLen - LeadInLen;I++,J--)
		{
			Decay = (DLReal) pow((DLReal)0.5,I / DecayTime);
			Sample = (DLReal) sin(S1 * (exp(J * S2) - 1.0));
			RMSV = (DLReal) (Sample * Decay);
			RMS += RMSV * RMSV;
		}

	/* Initial lead in */
	for (I = SweepLen - LeadInLen,J = LeadInLen;I < SweepLen;I++,J--)
		{
			Decay = (DLReal) pow((DLReal)0.5,I / DecayTime);
			Sample = (DLReal) sin(S1 * (exp(J * S2) - 1.0));
			WC = (DLReal) (0.42 - 0.5 * cos(WC1In * J) + 0.08 * cos(WC2In * J));
			RMSV = (DLReal) (Sample * WC * Decay);
			RMS += RMSV * RMSV;
		}

	/* Computes the normalization factor */
	printf("Normalizaton factor: %f\n",RMS);
	fflush(stdout);
	RMS = (DLReal) 0.5 / RMS;

	/* Open the inverse file */
    OF = inverseFile;

	/* Generates the sweep file */
	cout << "Generating the inverse file...\n";

	/* Final lead out */
	for (I = 0,J = SweepLen;I < LeadOutLen;I++,J--)
		{
			Decay = (DLReal) pow((DLReal)0.5,I / DecayTime);
			Sample = (DLReal) sin(S1 * (exp(J * S2) - 1.0));
			WC = (DLReal) (0.42 - 0.5 * cos(WC1Out * I) + 0.08 * cos(WC2Out * I));
			FS = (float) (RMS * Sample * WC * Decay);
			fwrite(&FS,sizeof(float),1,OF);
		}

	/* Full sweep */
	for (I = LeadOutLen,J = SweepLen - LeadOutLen;I < SweepLen - LeadInLen;I++,J--)
		{
			Decay = (DLReal) pow((DLReal)0.5,I / DecayTime);
			Sample = (DLReal) sin(S1 * (exp(J * S2) - 1.0));
			FS = (float) (RMS * Sample * Decay);
			fwrite(&FS,sizeof(float),1,OF);
		}

	/* Initial lead in */
	for (I = SweepLen - LeadInLen,J = LeadInLen;I < SweepLen;I++,J--)
		{
			Decay = (DLReal) pow((DLReal)0.5,I / DecayTime);
			Sample = (DLReal) sin(S1 * (exp(J * S2) - 1.0));
			WC = (DLReal) (0.42 - 0.5 * cos(WC1In * J) + 0.08 * cos(WC2In * J));
			FS = (float) (RMS * Sample * WC * Decay);
			fwrite(&FS,sizeof(float),1,OF);
		}

	/* Close the inverse file */
	cout << "Inverse file generated.\n";

	/* Execution completed */
	return 0;
}





/*******************************************************************************/
/* Generate Impulse response from recorded sweep */
/*******************************************************************************/


size_t FSize(FILE *F)
{
	long FS, FSPrev=0;

    FSPrev = ftell(F);
	fseek(F,0,SEEK_END);
    if ( (FS = ftell(F)) == -1) {
        cerr << "Error determining file length\n";
        exit (-1);;
    }
    fseek(F, FSPrev, SEEK_SET);
	return (size_t) FS;
}

/* Calola il valore RMS del segnale Sig */
DLReal GetRMSLevel(const DLReal * Sig,const int SigLen)
{
	DLReal RMS;
	int I;

	RMS = 0;
	for (I = 0; I < SigLen; I++)
		RMS += Sig[I] * Sig[I];

	return (DLReal) sqrt(RMS);
}

/* Halfcomplex array convolution */
void hcconvolve(DLReal * A,const DLReal * B,const int N)
{
	int R;
	int I;
	DLReal T1;
	DLReal T2;

	A[0] *= B[0];
	A[1] *= B[1];
	for (R = 2,I = 3;R < N;R += 2,I += 2)
		{
			T1 = A[R] * B[R];
			T2 = A[I] * B[I];
			A[I] = ((A[R] + A[I]) * (B[R] + B[I])) - (T1 + T2);
			A[R] = T1 - T2;
		}
}

/******************************************************************
* Take the Log Sweep recording and convolve with the InverseFile
* If there is a reference channel then use that as well
*******************************************************************/
int LSConv(	FILE * SweepFile, FILE * InverseFile,
	        FILE * OutFile, FILE * RefSweepFile,
	        DLReal MinGain, DLReal DLStart)
{

    /* Convolution parameters */
	int SS;
	int IS;
	int RS;
	int CS;
	int CL;
	int I;
	int J;
	int K;
	int L;
	DLReal * Sweep;
	DLReal * Inverse;
	float RF;

	/* Dip limiting */
	DLReal RMSLevel;
	DLReal RMSMin;
	DLReal DLSLevel;
	DLReal DLLevel;
	DLReal DLGFactor;
	DLReal DLMFactor;
	DLReal DLAbs;
	DLReal DLMin;
	DLReal DLFMin;
	DLReal DLLMin;
	DLReal DLTheta;

	/* Peak position */
	int MP;
	DLReal AMax;

	/* Input/output file */
	FILE * IOF;


	/* Computes the convolution length */
	cout << "Convolution length computation.\n";
	SS = FSize(SweepFile) / sizeof(float);
	IS = FSize(InverseFile) / sizeof(float);
	if (RefSweepFile != NULL)
		RS = FSize(RefSweepFile) / sizeof(float);
	else
		RS = 1;
	CL = SS + IS + RS - 2;
	for(CS = 1;CS < CL;CS <<= 1);

	/* Convolution arrays allocation */
	cout << "Convolution arrays allocation.\n";
	if ((Sweep = (DLReal *) malloc(sizeof(DLReal) * CS)) == NULL)
		{
			cerr << "Memory allocation failure.\n";
			return 1;
		}
	if ((Inverse = (DLReal *) malloc(sizeof(DLReal) * CS)) == NULL)
		{
			cerr << "Memory allocation failure.\n";
			free(Sweep);
			return 1;
		}

	/* Read the inverse file */
	cout << "Reading inverse file: " << InverseFile << "\n";
	IOF = InverseFile;
    rewind(IOF);
	for(I = 0;I < IS;I++)
		{
			fread(&RF,sizeof(float),1,IOF);
			Inverse[I] = (DLReal) RF;
		}
	for (I = IS;I < CS;I++)
		Inverse[I] = (DLReal) 0.0;

	cout << "Inverse filter FFT...\n";
	rdft(CS,OouraRForward,Inverse);

	/* Check the reference file */
	if (RefSweepFile != NULL)
		{
			/* Read the reference file */
			cout << "Reading reference file: " << RefSweepFile << "\n";
			IOF = RefSweepFile;
            rewind(IOF);
			for (I = 0;I < CS;I++)
				Sweep[I] = (DLReal) 0.0;
			for(I = CS - IS,J = 0;J < RS;I++,J++)
				{
					fread(&RF,sizeof(float),1,IOF);
					Sweep[I % CS] = (DLReal) RF;
				}

			/* Convolving sweep and inverse */
			cout << "Reference inversion and convolution...\n";
			rdft(CS,OouraRForward,Sweep);
			hcconvolve(Sweep,Inverse,CS);

			/* Computes the RMS Level */
			RMSLevel = Sweep[0] * Sweep[0];
			for (I = 2,J = 3;I < CS;I += 2,J += 2)
				RMSLevel += Sweep[I] * Sweep[I] + Sweep[J] * Sweep[J];
			RMSLevel = (DLReal) sqrt(2.0 * RMSLevel / CS);
			RMSMin = MinGain * RMSLevel;

			/* Check the gain truncation type */
			if (DLStart >= (DLReal) 1.0)
				{
					/* Check starting components */
					if (Sweep[0] < RMSMin)
						Sweep[0] = RMSLevel / RMSMin;
					else
						Sweep[0] = RMSLevel / Sweep[0];
					if (Sweep[1] < RMSMin)
						Sweep[1] = RMSLevel / RMSMin;
					else
						Sweep[1] = RMSLevel / Sweep[1];

					/* Gain truncation scan */
					for (I = 2,J = 3;I < CS;I += 2,J += 2)
						{
							/* Gain computation */
							DLAbs = (DLReal) hypot(Sweep[I],Sweep[J]);

							/* Gain check and limitation */
							if (DLAbs < RMSMin)
								{
									/* Gain limited inversion */
									DLTheta = (DLReal) atan2(Sweep[J],Sweep[I]);
									DLAbs = RMSLevel / RMSMin;
									Sweep[I] = DLAbs * (DLReal) cos(-DLTheta);
									Sweep[J] = DLAbs * (DLReal) sin(-DLTheta);
								}
							else
								{
									/* Inversion and renormalization */
									DLAbs = RMSLevel / (DLAbs * DLAbs);
									Sweep[I] *= DLAbs;
									Sweep[J] *= -DLAbs;
								}
						}
				}
			else
				{
					/* Determina i fattori per la limitazione guadagno */
					DLSLevel = RMSMin / DLStart;
					DLGFactor = DLSLevel - RMSMin;
					DLMin = (DLReal) -1.0;
					DLFMin = (DLReal) -1.0;

					/* Scansione per limitazione guadagno */
					for (I = 2,J = 3;I < CS;I += 2,J += 2)
						{
							DLAbs = (DLReal) hypot(Sweep[I],Sweep[J]);
							if (DLAbs <= (DLReal) 0.0)
								{
									Sweep[I] = RMSLevel / RMSMin;
									Sweep[J] = 0;
									if (DLMin >= (DLReal) 0.0)
										{
											DLLMin = DLMin;
											DLMin = (DLReal) -1.0;
										}
								}
							else
								if (DLAbs < DLSLevel)
									{
										/* Verifica se � gi� disponibile il minimo locale */
										if (DLMin < (DLReal) 0.0)
											{
												/* Cerca il minimo locale */
												DLMin = DLAbs;
												DLLevel = DLAbs;
												for (K = I + 2,L = I + 3;K < CS && DLLevel < DLSLevel;K += 2,L += 2)
													{
														if (DLLevel < DLMin)
															DLMin = DLLevel;
														DLLevel = (DLReal) hypot(Sweep[K],Sweep[L]);
													}

												/* Salva il primo minimo */
												if (DLFMin < (DLReal) 0.0)
													DLFMin = DLMin;

												/* Verifica se il minimo locale � inferiore
												al livello minimo e ricalcola i fattori di
												compressione */
												if (DLMin < RMSMin)
													DLMFactor = DLSLevel - DLMin;
												else
													DLMFactor = DLSLevel - RMSMin;
											}

										/* Riassegna il guadagno del filtro */
										DLLevel = (DLSLevel - DLAbs) / DLMFactor;
										DLAbs = RMSLevel / (DLSLevel - DLGFactor * DLLevel);

										/* Gain limited inversion */
										DLTheta = (DLReal) atan2(Sweep[J],Sweep[I]);
										Sweep[I] = DLAbs * (DLReal) cos(-DLTheta);
										Sweep[J] = DLAbs * (DLReal) sin(-DLTheta);
									}
								else
									{
										if (DLMin >= (DLReal) 0.0)
											{
												DLLMin = DLMin;
												DLMin = (DLReal) -1.0;
											}

										/* Inversion and renormalization */
										DLAbs = RMSLevel / (DLAbs * DLAbs);
										Sweep[I] *= DLAbs;
										Sweep[J] *= -DLAbs;
									}
						}

					/* Check starting components */
					if (Sweep[0] < DLSLevel)
						{
							/* Verifica se il minimo locale � inferiore
							al livello minimo e ricalcola i fattori di
							compressione */
							if (DLFMin < RMSMin)
								DLMFactor = DLSLevel - DLFMin;
							else
								DLMFactor = DLSLevel - RMSMin;

							/* Riassegna il guadagno del filtro */
							DLLevel = (DLSLevel - DLAbs) / DLMFactor;
							Sweep[0] = RMSLevel / (DLSLevel - DLGFactor * DLLevel);
						}
					else
						Sweep[0] = (DLReal) RMSLevel / Sweep[0];

					if (Sweep[1] < DLSLevel)
						{
							/* Verifica se il minimo locale � inferiore
							al livello minimo e ricalcola i fattori di
							compressione */
							if (DLLMin < RMSMin)
								DLMFactor = DLSLevel - DLLMin;
							else
								DLMFactor = DLSLevel - RMSMin;

							/* Riassegna il guadagno del filtro */
							DLLevel = (DLSLevel - DLAbs) / DLMFactor;
							Sweep[1] = RMSLevel / (DLSLevel - DLGFactor * DLLevel);
						}
					else
						Sweep[1] = (DLReal) RMSLevel / Sweep[1];
				}

			/* Inverse convolution */
			hcconvolve(Inverse,Sweep,CS);
		}

	/* Read the sweep file */
	cout << "Reading sweep file: " << SweepFile << "\n";
	IOF = SweepFile;
    rewind(IOF);
	for(I = 0;I < SS;I++)
		{
			fread(&RF,sizeof(float),1,IOF);
			Sweep[I] = (DLReal) RF;
		}
	for (I = SS;I < CS;I++)
		Sweep[I] = (DLReal) 0.0;

	/* Convolving sweep and inverse */
	cout << "Sweep and inverse convolution...\n";
	rdft(CS,OouraRForward,Sweep);
	hcconvolve(Sweep,Inverse,CS);

	/* Impulse response recover */
	rdft(CS,OouraRBackward,Sweep);
	for (I = 0;I < CS;I++)
		Sweep[I] *= (DLReal) (2.0  / CS);

	/* Peak search */
	cout << "Finding impulse response peak value...\n";
	MP = 0;
	AMax = (DLReal) 0.0;
	for (I = 0;I < CS;I++)
		if ((DLReal) fabs(Sweep[I]) > AMax)
			{
				MP = I;
				AMax = (DLReal) fabs(Sweep[I]);
			}
	printf("Peak position: %d\n",MP);
	if (AMax > (DLReal) 0.0)
		printf("Peak value: %f (%f dB)\n",(double) AMax, (double) (20 * log10((double) AMax)));
	else
		printf("Peak value: %f (-inf dB)\n",(double) AMax);
	fflush(stdout);


	/* Writes output file */
	cout << "Writing output file: " << OutFile << "\n";
	IOF = OutFile;
    rewind(IOF);
	for(I = 0;I < CL;I++)
		{
			RF = (float) Sweep[I];
			fwrite(&RF,sizeof(float),1,IOF);
		}

	/* Memory deallocation */
	free(Sweep);
	free(Inverse);

	/* Execution completed */
	cout << "Completed.\n";
	return 0;
}


/*******************************************************************************/
/* Main code */
/*******************************************************************************/

void usage(void) {
  /* Error function in case of incorrect command-line
     argument specifications
  */
  cerr << "\nusage: rec_imp impulseFile rate hzStart hzEnd duration sweepOut sweepIn [loopOut loopIn mingain [dlstart]]\n";
  cerr << "    where options,\n";
  cerr << "    impulseFile = file to save impulse response to,\n";
  cerr << "    rate = the sample rate,\n";
  cerr << "    hzStart = freq to start sweep,\n";
  cerr << "    hzEnd = freq to end sweep,\n";
  cerr << "    duration = length of sweep in seconds,\n";
  cerr << "    sweepOut = channel and device to play the sweep,\n";
  cerr << "            syntax: channel:device, device=0 automatically picks first card,\n";
  cerr << "    sweepIn = channel and device to record the sweep,\n";
  cerr << "            syntax: channel:device, device=0 automatically picks first card,\n";
  cerr << "    loopOut = reference channel to play loopback signal.\n";
  cerr << "    loopIn = reference channel to record loopback signal.\n";
  cerr << "            OR non numeric file name of previously recorded loopback signal\n";
  cerr << "    minGain = min gain for reference channel inversion,\n";
  cerr << "    dlstart = dip limiting start for reference channel inversion\n";
  cerr << "\n eg: rec_imp rec.pcm 44100 10 21000 45 0:0 0:0 1:0 1:0 0.1\n\n";
  cerr << "\nOR: rec_imp -l\n";
  cerr << "    Lists all devices detected.\n\n";

//  exit(0);
}

// Parses a param of the type int:int, eg 1:0
// Returns 0 on success, -1 on error
int parseParam(char *param, int *value1, int *value2)
{
    if ( sscanf(param, "%i:%i", value1, value2) != 2)
        return (-1); //failed to parse

    return 0;
}

int main(int argc, char *argv[])
{
    char *impFileName;
    int chanSweepIn, chanSweepOut;
    int chanLoopIn = -1, chanLoopOut = -1;
    int deviceIn = 0, deviceOut = 0;
    int rate, hzStart, hzEnd, duration;
    DLReal minGain = 0, dlStart = (DLReal)2;

	/* Initial message */
	cout << "\nrec_imp 1.0.0: generates a log sweep and inverse filter, plays and records it, and then generates an impulse response file.\n";
	cout << "Copyright (C) 2002-2004 Denis Sbragion, 2004 Edward Wildgoose\n";
	#ifdef UseDouble
		cout << "\nCompiled with double precision arithmetic.\n";
	#else
		cout << "\nCompiled with single precision arithmetic.\n";
	#endif
	cout << "\nThis program may be freely redistributed under the terms of\n";
	cout << "the GNU GPL and is provided to you as is, without any warranty\n";
	cout << "of any kind. Please read the file \"COPYING\" for details.\n";


    // minimal command-line checking...
    if (argc==2 && strcmp(argv[1],"-l")==0) {
        showDeviceInfo();
        return (0);
    } else if (argc!=8 && argc!=11 && argc!=12) {
        usage();
        return(-1);
    }

    //Parse command line params
    // Hard to be consistent, but all "out" vars refer to playing from disk file to soundcard
    // Likewise "in" vars refer to recording from soundcard to disk

    impFileName = argv[1];
    rate = (int) atoi(argv[2]);
    hzStart= (int) atoi(argv[3]);
    hzEnd= (int) atoi(argv[4]);
    duration= (int) atoi(argv[5]);
    if ( parseParam(argv[6], &chanSweepOut, &deviceOut) ) {
        cout << "error parsing LoopOut param\n";
        exit(-1);
    }
    if ( parseParam(argv[7], &chanSweepIn, &deviceIn) ) {
        cout << "error parsing LoopOut param\n";
        exit(-1);
    }
    if (argc >= 9) {
        chanLoopOut = (int) atoi(argv[8]);
        chanLoopIn = (int) atoi(argv[9]);
        minGain = (DLReal) atof(argv[10]);
    }
    if (argc >= 12) {
        dlStart = (DLReal) atof(argv[11]);
    }

    // Generate the sweep files
    FILE *sweep = getTempFile("drcsweep");
    FILE *invSweep = getTempFile("drcinv");
    FILE *recSweep = getTempFile("drcrec");
    FILE *loopSweep = NULL;
    if (chanLoopIn >= 0) loopSweep = getTempFile("drcloop");

    generateSweep(sweep, invSweep, rate, hzStart, hzEnd, duration, 2);

    fflush(sweep);
    fflush(invSweep);
#ifndef WIN32
    fsync(fileno(sweep));
    fsync(fileno(invSweep));
#endif
    rewind(sweep);
    rewind(invSweep);

    // Do the biz and record the sweep
    playRecord(recSweep, sweep, loopSweep, rate, deviceIn, deviceOut, chanSweepIn, chanSweepOut, chanLoopIn, chanLoopOut);

    rewind(recSweep);
    if (loopSweep != NULL) rewind(loopSweep);

    FILE *impulse = FOpen(impFileName, "w+b");
    LSConv(recSweep, invSweep, impulse, loopSweep, minGain, dlStart);

    // Cleanup temp files
    fclose(sweep);
    fclose(invSweep);
    fclose(impulse);
    if (loopSweep != NULL) fclose(loopSweep);

    return (0);
}

