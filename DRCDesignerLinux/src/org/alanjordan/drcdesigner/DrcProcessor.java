/*
  Copyright 2011 Alan Brent Jordan
  This file is part of Digital Room Correction Designer.

  Digital Room Correction Designer is free software: you can redistribute 
  it and/or modify it under the terms of the GNU General Public License 
  as published by the Free Software Foundation, version 3 of the License.

  Digital Room Correction Designer is distributed in the hope that it will
  be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General 
  Public License for more details.

  You should have received a copy of the GNU General Public License along with 
  Digital Room Correction Designer.  If not, see <http://www.gnu.org/licenses/>.
 */

package org.alanjordan.drcdesigner;

//import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
//import java.io.FileReader;
import java.io.FileWriter;
//import java.io.IOException;
import java.io.PrintWriter;
import java.util.Scanner;

public class DrcProcessor extends Thread {

    public enum FilterType {erb, minimal, soft, normal, strong};
    public enum SamplingRate {_44100, _48000, _88200, _96000};
	private Options options;
	private StandardFiltersPanel parentWindow;
	private String samplingRate;
	private String impulseCenter;
	
    public DrcProcessor(Options options, StandardFiltersPanel parentWindow, String samplingRate) {
        this.options = options;
        this.parentWindow = parentWindow;
        this.samplingRate = samplingRate;
    }

    @Override
    public void run() {
		parentWindow.enableDisableGenerateFiltersButton(false);
        try { sleep(1);} catch (InterruptedException ie) {}
		
        Targets t = new Targets(options);
        t.writeTargetPointsFile(Integer.parseInt(samplingRate));

        if (parentWindow.checkSelectedFilterType(FilterType.erb))
        	runDrc(FilterType.erb);
        if (parentWindow.checkSelectedFilterType(FilterType.minimal))
        	runDrc(FilterType.minimal);
        if (parentWindow.checkSelectedFilterType(FilterType.soft))
        	runDrc(FilterType.soft);
        if (parentWindow.checkSelectedFilterType(FilterType.normal))
        	runDrc(FilterType.normal);
        if (parentWindow.checkSelectedFilterType(FilterType.strong))
        	runDrc(FilterType.strong);
        
    	parentWindow.setStatus("Finished generating filters");
        try { sleep(1);} catch (InterruptedException ie) {}
        
		parentWindow.enableDisableGenerateFiltersButton(true);
        
    }



    private void runDrc(FilterType fType) {
		String convolverDir = options.getRoomCorrectionRootPath() + "/ConvolverFilters";
		String micCompBehavior = "";
		
		if (options.isUseMicCompensationFile() && options.getMicCompensationFile() != null) {
			micCompBehavior = "--MCFilterType=M --MCPointsFile=\"" + options.getMicCompensationFilePath() + "\" ";
		}
		
		parentWindow.setStatus("Generating left channel " + fType.toString() + " " + samplingRate + " filter");
        try { sleep(1);} catch (InterruptedException ie) {}

		try {
        	PrintWriter out = new PrintWriter(new FileWriter(options.getBatchFilesPath() + "/drcWrapperRunDRCLeft" + fType + "_" + samplingRate +  ".bat", false));
        	out.println("#!/bin/bash");
//        	out.println("drc " + micCompBehavior + "--PSPointsFile=" + options.getDrcFilesPath() + "/pa-" + samplingRate + ".txt " + "--BCInFile=" + options.getOutputFilesPath() + "/LeftSpeakerImpulseResponse" + samplingRate + ".pcm --PSOutFile=" + options.getOutputFilesPath()+ "/LeftSpeaker" + samplingRate + fType.toString().toUpperCase() + ".pcm " + options.getDrcFilesPath() + "/" + fType.toString() + samplingRate + ".drc");

        	out.println("drc " + micCompBehavior + "--PSPointsFile="+ options.getRoomCorrectionRootPath() + "/Targets/DRCDesignerCustomizedPoints.txt " + "--BCInFile=" + options.getOutputFilesPath() + "/LeftSpeakerImpulseResponse" + samplingRate + ".pcm --PSOutFile=" + options.getOutputFilesPath()+ "/LeftSpeaker" + samplingRate + fType.toString().toUpperCase() + ".pcm " + options.getDrcFilesPath() + "/" + fType.toString() + samplingRate + ".drc");  
        	out.println("wait");
        	out.println("mv -f " + options.getOutputFilesPath() + "/LeftSpeaker" + samplingRate + fType.toString().toUpperCase() + ".pcm " + convolverDir);
        	out.close();
        	
        	String[] command = new String[] {"chmod", "a+x", options.getBatchFilesPath() + "/drcWrapperRunDRCLeft" + fType + "_" + samplingRate +  ".bat"};
        	Runtime rt = Runtime.getRuntime();
        	Process p = rt.exec(command);
    		p.waitFor();

    		String resultsFileName = options.getOutputFilesPath() + "/drcOutputLeft" + samplingRate + fType.toString() + ".txt";
    		command = new String[] {"bash", "-c", options.getBatchFilesPath() + "/drcWrapperRunDRCLeft" + fType.toString() + "_" + samplingRate + ".bat 1>" + resultsFileName + " 2>&1"};
    		rt = Runtime.getRuntime();
    		Process p1 = rt.exec(command);
    		p1.waitFor();

    		parentWindow.setStatus("Parsing results to find center");
            try { sleep(1);} catch (InterruptedException ie) {}
            impulseCenter = parseResultsFileForImpulseCenter(resultsFileName);

    		parentWindow.setStatus("Generating right channel " + fType.toString() + " configuration file");
            try { sleep(1);} catch (InterruptedException ie) {}
            
//            generateRightChannelConfigurationFile(drcDir + "/" + fType.toString() + samplingRate + "RightChannelTemplate.drc", drcDir + "/" + fType.toString() + samplingRate + "RightChannel.drc");

    		parentWindow.setStatus("Generating right channel " + fType.toString() + " " + samplingRate + " filter");
            try { sleep(1);} catch (InterruptedException ie) {}

        	out = new PrintWriter(new FileWriter(options.getBatchFilesPath() + "/drcWrapperRunDRCRight" + fType + "_" + samplingRate +  ".bat", false));
        	out.println("#!/bin/bash");
        	out.println("drc "  + micCompBehavior + "--PSPointsFile=" + options.getRoomCorrectionRootPath() + "/Targets/DRCDesignerCustomizedPoints.txt " + "--BCInFile=" + options.getOutputFilesPath()+ "/RightSpeakerImpulseResponse" + samplingRate + ".pcm --PSOutFile=" + options.getOutputFilesPath() + "/RightSpeaker" + samplingRate + fType.toString().toUpperCase() + ".pcm --BCImpulseCenterMode=M --BCImpulseCenter=" + impulseCenter + " " + options.getDrcFilesPath() + "/" + fType.toString() + samplingRate + ".drc");
        	out.println("wait");
        	out.println("mv -f " + options.getOutputFilesPath() + "/RightSpeaker" + samplingRate + fType.toString().toUpperCase() + ".pcm " + convolverDir);
        	out.close();
        	resultsFileName = options.getOutputFilesPath() + "/drcOutputRight" + samplingRate + fType.toString() + ".txt";
  
    		command = new String[] {"chmod", "a+x", options.getBatchFilesPath() + "/drcWrapperRunDRCRight" + fType + "_" + samplingRate +  ".bat"};
    		rt = Runtime.getRuntime();
    		Process p2 = rt.exec(command);
    		p2.waitFor();

        	
        	command = new String[] {"bash", "-c", options.getBatchFilesPath() + "/drcWrapperRunDRCRight" + fType + "_" + samplingRate + ".bat 1>" + resultsFileName + " 2>&1"};
    		rt = Runtime.getRuntime();
    		Process p3 = rt.exec(command);
    		p3.waitFor();
    		
    		generateConvolverConfigFile(fType);
    		
    		generateWavFile(samplingRate, fType);

    	}
    	catch(Exception exc){
    		exc.printStackTrace();
    	}		
    	
    }
    
	private void generateWavFile(String samplingRate, FilterType fType) {
        String convolverDir = options.getRoomCorrectionRootPath() + "/ConvolverFilters/";
        String leftPcmFilePath = convolverDir + "LeftSpeaker" + samplingRate + fType.toString().toUpperCase() + ".pcm";
        String rightPcmFilePath = convolverDir + "RightSpeaker" + samplingRate + fType.toString().toUpperCase() + ".pcm";
        String outputWavFilePath = convolverDir + "Stereo" + samplingRate + fType.toString().toUpperCase() + ".wav";
       
        parentWindow.setStatus("Generating custom " + samplingRate + " stereo WAV file");
        try { sleep(1);} catch (InterruptedException ie) {}

        SoxProcessor sp = new SoxProcessor(options);
        sp.createWavFromRawPcm(leftPcmFilePath, rightPcmFilePath, outputWavFilePath, samplingRate, false);
    }

    
    private void generateConvolverConfigFile(FilterType fType) {

		parentWindow.setStatus("Generating " + fType.toString() + " " + samplingRate + " convolver configuration");
        try { sleep(1);} catch (InterruptedException ie) {}

		try {
		      PrintWriter out = new PrintWriter(new FileWriter(options.getRoomCorrectionRootPath() + "/JConvolverConfigs/" + "jconvolver" + fType.toString().toUpperCase() + samplingRate + ".conf", false));
		      out.println("/cd " + options.getRoomCorrectionRootPath() + "/ConvolverFilters");
		      out.println();
		      out.println("#                in  out   partition    maxsize");
		      out.println("# ---------------------------------------------");
		      out.println("/convolver/new   2   2     8192         524426");
		      out.println();
		      out.println("#              num   port name     connect to"); 
		      out.println("# -----------------------------------------------");
		      out.println("/input/name    1     Input.L");
		      out.println("/input/name    2     Input.R");
		      out.println();
		      out.println("/output/name   1     Output.L");
		      out.println("/output/name   2     Output.R");
		      out.println();
		      out.println("#               in out  gain  delay  offset  length  chan      file");  
		      out.println("# -------------------------------------------------------------------------------");
		      out.println("/impulse/read   1  1    1     0      0       0       1         Stereo" + samplingRate + fType.toString().toUpperCase() + ".wav");
		      out.println("/impulse/read   2  2    1     0      0       0       2         Stereo" + samplingRate + fType.toString().toUpperCase() + ".wav");
		      out.close();
		}
    	catch(Exception exc){
    		exc.printStackTrace();
    	}		   	
    }
    
	private String parseResultsFileForImpulseCenter(String fileName) {
        Scanner scanner;
        String line;
        int startingChar = 0;
        int endingChar = 0;
        String result = "Impulse Center Not Found";
        
        try {
            scanner = new Scanner(new File(fileName));

            while (scanner.hasNextLine()) {
                line = scanner.nextLine();
                if (line.startsWith("Impulse center found at sample ")) {
                	startingChar = 31;
                	endingChar = line.indexOf(".");
                	result =  line.substring(startingChar, endingChar);
                }
            }
            scanner.close();
        }
        catch (FileNotFoundException fnf) {
            System.out.println(fileName + " not found");
        }
        return result;
	}

    
    
//    private void generateRightChannelConfigurationFile(String templateFileName, String outputFileName) {
//            try {
//           
//                // Create FileReader Object
//                FileReader inputFileReader   = new FileReader(templateFileName);
//                FileWriter outputFileReader  = new FileWriter(outputFileName, false);
//
//                // Create Buffered/PrintWriter Objects
//                BufferedReader inputStream   = new BufferedReader(inputFileReader);
//                PrintWriter    outputStream  = new PrintWriter(outputFileReader);
//
//                String inLine = null;
//
//                while ((inLine = inputStream.readLine()) != null) {
//                    if (inLine.startsWith("BCImpulseCenter =")) {
//                        outputStream.println("BCImpulseCenter = " + impulseCenter);
//                    }
//                    else {
//                        outputStream.println(inLine);
//                    }
//                }
//                outputStream.close();
//                inputStream.close();
//                
//            } catch (IOException e) {
//
//                System.out.println(e.getMessage());
//                e.printStackTrace();
//            }
//    }

    
	// Working version below for generating right channel DRC configuration file
//  private void runDrc(FilterType fType) {
//		String drcDir = options.getRoomCorrectionRootPath() + "/drc-3.1.1/sample";
//		String convolverDir = options.getRoomCorrectionRootPath() + "/ConvolverFilters";
//		
//		parentWindow.setStatus("Generating left channel " + fType.toString() + " " + samplingRate + " filter");
//      try { sleep(1);} catch (InterruptedException ie) {}
//
//		try {
//      	PrintWriter out = new PrintWriter(new FileWriter("drcWrapperRunDRCLeft" + fType + "_" + samplingRate +  ".bat", false));
//      	out.println("cd " + drcDir);
//      	out.println("drc.exe --BCInFile=LeftSpeakerImpulseResponse" + samplingRate + ".pcm --PSOutFile=LeftSpeaker" + samplingRate + fType.toString().toUpperCase() + ".pcm " + fType.toString() + samplingRate + ".drc");
//      	out.println("move /y LeftSpeaker" + samplingRate + fType.toString().toUpperCase() + ".pcm " + convolverDir);
//      	out.close();
//      	
//      	String resultsFileName = "drcOutputLeft" + samplingRate + fType.toString() + ".txt";
//  		String command = "cmd.exe /c \"drcWrapperRunDRCLeft" + fType.toString() + "_" + samplingRate + ".bat 1>" + resultsFileName + " 2>&1\"";
//  		Runtime rt = Runtime.getRuntime();
//  		Process p = rt.exec(command);
//  		p.waitFor();
//
//  		parentWindow.setStatus("Parsing results to find center");
//          try { sleep(1);} catch (InterruptedException ie) {}
//          impulseCenter = parseResultsFileForImpulseCenter(resultsFileName);
//
//  		parentWindow.setStatus("Generating right channel " + fType.toString() + " configuration file");
//          try { sleep(1);} catch (InterruptedException ie) {}
//          
//          generateRightChannelConfigurationFile(drcDir + "/" + fType.toString() + samplingRate + "RightChannelTemplate.drc", drcDir + "/" + fType.toString() + samplingRate + "RightChannel.drc");
//
//  		parentWindow.setStatus("Generating right channel " + fType.toString() + " " + samplingRate + " filter");
//          try { sleep(1);} catch (InterruptedException ie) {}
//
//      	out = new PrintWriter(new FileWriter("drcWrapperRunDRCRight" + fType + "_" + samplingRate +  ".bat", false));
//      	out.println("cd " + drcDir);
//      	out.println("drc.exe --BCInFile=RightSpeakerImpulseResponse" + samplingRate + ".pcm --PSOutFile=RightSpeaker" + samplingRate + fType.toString().toUpperCase() + ".pcm " + fType.toString() + samplingRate + "RightChannel.drc");
//      	out.println("move /y RightSpeaker" + samplingRate + fType.toString().toUpperCase() + ".pcm " + convolverDir);
//      	out.close();
//      	resultsFileName = "drcOutputRight" + samplingRate + fType.toString() + ".txt";
//  		command = "cmd.exe /c \"drcWrapperRunDRCRight" + fType + "_" + samplingRate + ".bat 1>" + resultsFileName + " 2>&1\"";
//  		rt = Runtime.getRuntime();
//  		p = rt.exec(command);
//  		p.waitFor();
//  		
//  		generateConvolverConfigFile(fType);
//  	}
//  	catch(Exception exc){
//  		exc.printStackTrace();
//  	}		
//  	
//  }

}
