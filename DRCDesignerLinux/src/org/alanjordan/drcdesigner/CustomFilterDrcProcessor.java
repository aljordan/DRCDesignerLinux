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

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileWriter;
import java.io.PrintWriter;
import java.util.Scanner;


public class CustomFilterDrcProcessor extends Thread {

	private Options options;
	private CustomizedFilterPanel parentWindow;
	private String samplingRate;
	private String impulseCenter;
	private ConfigurationEntries configEntries;
	private int customFileNumber;

	
    public CustomFilterDrcProcessor(Options options, CustomizedFilterPanel parentWindow, String samplingRate, ConfigurationEntries configEntries) {
        this.options = options;
        this.parentWindow = parentWindow;
        this.samplingRate = samplingRate;
        this.configEntries = configEntries;
        customFileNumber = 1;
    }

    @Override
    public void run() {
		parentWindow.enableDisableGenerateFiltersButton(false);
        try { sleep(1);} catch (InterruptedException ie) {}
		
        Targets t = new Targets(options);
        t.writeTargetPointsFile(Integer.parseInt(samplingRate));

        runDrc();
        
    	parentWindow.setStatus("Finished generating filters");
        try { sleep(1);} catch (InterruptedException ie) {}
        
		parentWindow.enableDisableGenerateFiltersButton(true);
        
    }

    private String getCommandLineParameters() {
    	return "--" + ConfigurationEntries.EntryNames.BCInitWindow.toString() + "=" + configEntries.getValue(ConfigurationEntries.EntryNames.BCInitWindow) + " "
    	+ "--" + ConfigurationEntries.EntryNames.EPLowerWindow.toString() + "=" + configEntries.getValue(ConfigurationEntries.EntryNames.EPLowerWindow) + " "
    	+ "--" + ConfigurationEntries.EntryNames.EPPFFinalWindow.toString() + "=" + configEntries.getValue(ConfigurationEntries.EntryNames.EPPFFinalWindow) + " "
    	+ "--" + ConfigurationEntries.EntryNames.EPUpperWindow.toString() + "=" + configEntries.getValue(ConfigurationEntries.EntryNames.EPUpperWindow) + " "
    	+ "--" + ConfigurationEntries.EntryNames.EPWindowExponent.toString() + "=" + configEntries.getValue(ConfigurationEntries.EntryNames.EPWindowExponent) + " "
    	+ "--" + ConfigurationEntries.EntryNames.ISPELowerWindow.toString() + "=" + configEntries.getValue(ConfigurationEntries.EntryNames.ISPELowerWindow) + " "
    	+ "--" + ConfigurationEntries.EntryNames.ISPEUpperWindow.toString() + "=" + configEntries.getValue(ConfigurationEntries.EntryNames.ISPEUpperWindow) + " "
    	+ "--" + ConfigurationEntries.EntryNames.MPLowerWindow.toString() + "=" + configEntries.getValue(ConfigurationEntries.EntryNames.MPLowerWindow) + " "
    	+ "--" + ConfigurationEntries.EntryNames.MPPFFinalWindow.toString() + "=" + configEntries.getValue(ConfigurationEntries.EntryNames.MPPFFinalWindow) + " "
    	+ "--" + ConfigurationEntries.EntryNames.MPUpperWindow.toString() + "=" + configEntries.getValue(ConfigurationEntries.EntryNames.MPUpperWindow) + " "
    	+ "--" + ConfigurationEntries.EntryNames.MPWindowExponent.toString() + "=" + configEntries.getValue(ConfigurationEntries.EntryNames.MPWindowExponent) + " "
    	+ "--" + ConfigurationEntries.EntryNames.MSFilterDelay.toString() + "=" + configEntries.getValue(ConfigurationEntries.EntryNames.MSFilterDelay) + " "
    	+ "--" + ConfigurationEntries.EntryNames.PLMaxGain.toString() + "=" + configEntries.getValue(ConfigurationEntries.EntryNames.PLMaxGain) + " "
    	+ "--" + ConfigurationEntries.EntryNames.RTLowerWindow.toString() + "=" + configEntries.getValue(ConfigurationEntries.EntryNames.RTLowerWindow) + " "
    	+ "--" + ConfigurationEntries.EntryNames.RTOutWindow.toString() + "=" + configEntries.getValue(ConfigurationEntries.EntryNames.RTOutWindow) + " "
    	+ "--" + ConfigurationEntries.EntryNames.RTUpperWindow.toString() + "=" + configEntries.getValue(ConfigurationEntries.EntryNames.RTUpperWindow) + " "
    	+ "--" + ConfigurationEntries.EntryNames.RTWindowExponent.toString() + "=" + configEntries.getValue(ConfigurationEntries.EntryNames.RTWindowExponent) + " "
    	+ "--" + ConfigurationEntries.EntryNames.RTWindowGap.toString() + "=" + configEntries.getValue(ConfigurationEntries.EntryNames.RTWindowGap);		
    }

    private void calculateCustomFileNumber() {
		File configFile = new File(options.getRoomCorrectionRootPath() + "/ConvolverFilters/" + "convolverConfigCUSTOM" + samplingRate + "_" + customFileNumber + ".txt");
		while (configFile.exists()) {
			customFileNumber++;
			configFile = new File(options.getRoomCorrectionRootPath() + "/ConvolverFilters/" + "convolverConfigCUSTOM" + samplingRate + "_" + customFileNumber + ".txt");
		}
    }
    
    private void runDrc() {
    	calculateCustomFileNumber();
		String convolverDir = options.getRoomCorrectionRootPath() + "/ConvolverFilters";
		String micCompBehavior = "";
		
		if (options.isUseMicCompensationFile() && options.getMicCompensationFile() != null) {
			micCompBehavior = "--MCFilterType=M --MCPointsFile=\"" + options.getMicCompensationFilePath() + "\" ";
		}
		
		parentWindow.setStatus("Generating left channel custom " + samplingRate + " filter");
        try { sleep(1);} catch (InterruptedException ie) {}

		try {
        	PrintWriter out = new PrintWriter(new FileWriter(options.getBatchFilesPath() + "/drcWrapperRunDRCLeftcustom_" + samplingRate +  ".bat", false));
        	out.println("drc " + micCompBehavior + "--PSPointsFile="+ options.getRoomCorrectionRootPath() + "/Targets/DRCDesignerCustomizedPoints.txt " + "--BCInFile=" + options.getOutputFilesPath() + "/LeftSpeakerImpulseResponse" + samplingRate + ".pcm --PSOutFile=" + options.getOutputFilesPath() + "/LeftSpeaker" + samplingRate + "CUSTOM" + "_" + customFileNumber + ".pcm " + getCommandLineParameters() + " " + options.getDrcFilesPath() + "/soft" + samplingRate + ".drc");
        	out.println("wait");
        	out.println("mv -f " + options.getOutputFilesPath() + "/LeftSpeaker" + samplingRate + "CUSTOM" + "_" + customFileNumber + ".pcm " + convolverDir);
        	out.close();

        	String[] command = new String[] {"chmod", "a+x", options.getBatchFilesPath() + "/drcWrapperRunDRCLeftcustom_" + samplingRate + ".bat"};
        	Runtime rt = Runtime.getRuntime();
        	Process p = rt.exec(command);
    		p.waitFor();

        	String resultsFileName = options.getOutputFilesPath() + "/drcOutputLeft" + samplingRate + "custom.txt";
    		command = new String[] {"bash", "-c", options.getBatchFilesPath() + "/drcWrapperRunDRCLeft" + "custom_" + samplingRate + ".bat 1>" + resultsFileName + " 2>&1"};
    		rt = Runtime.getRuntime();
    		p = rt.exec(command);
    		p.waitFor();

    		parentWindow.setStatus("Parsing results to find center");
            try { sleep(1);} catch (InterruptedException ie) {}
            impulseCenter = parseResultsFileForImpulseCenter(resultsFileName);
            
    		parentWindow.setStatus("Generating right channel custom " + samplingRate + " filter");
            try { sleep(1);} catch (InterruptedException ie) {}

        	out = new PrintWriter(new FileWriter(options.getBatchFilesPath() + "/drcWrapperRunDRCRightcustom_" + samplingRate +  ".bat", false));
        	out.println("drc " + micCompBehavior + "--PSPointsFile="+ options.getRoomCorrectionRootPath() + "/Targets/DRCDesignerCustomizedPoints.txt " + "--BCInFile=" + options.getOutputFilesPath() + "/RightSpeakerImpulseResponse" + samplingRate + ".pcm --PSOutFile=" + options.getOutputFilesPath() + "/RightSpeaker" + samplingRate + "CUSTOM" + "_" + customFileNumber + ".pcm --BCImpulseCenterMode=M --BCImpulseCenter=" + impulseCenter + " " + getCommandLineParameters() + " " + options.getDrcFilesPath() + "/soft" + samplingRate + ".drc");
        	out.println("mv -f " + options.getOutputFilesPath() + "/RightSpeaker" + samplingRate + "CUSTOM" + "_" + customFileNumber + ".pcm " + convolverDir);
        	out.close();
        	
    		command = new String[] {"chmod", "a+x", options.getBatchFilesPath() + "/drcWrapperRunDRCRightcustom_" + samplingRate +  ".bat"};
    		rt = Runtime.getRuntime();
    		Process p2 = rt.exec(command);
    		p2.waitFor();
       	
        	resultsFileName = options.getOutputFilesPath() + "/drcOutputRight" + samplingRate + "custom.txt";
    		command = new String[] {"bash", "-c",  options.getBatchFilesPath() + "/drcWrapperRunDRCRight" + "custom_" + samplingRate + ".bat 1>" + resultsFileName + " 2>&1"};
    		rt = Runtime.getRuntime();
    		p = rt.exec(command);
    		p.waitFor();
    		
    		generateConvolverConfigFile();
    		
    		generateWavFile(samplingRate);
		}
    	catch(Exception exc){
    		exc.printStackTrace();
    	}		
    	
    }
    
	private void generateWavFile(String samplingRate) {
        String convolverDir = options.getRoomCorrectionRootPath() + "/ConvolverFilters/";
        String leftPcmFilePath = convolverDir + "LeftSpeaker" + samplingRate + "CUSTOM" + "_" + customFileNumber + ".pcm";
        String rightPcmFilePath = convolverDir + "RightSpeaker" + samplingRate + "CUSTOM" + "_" + customFileNumber + ".pcm";
        String outputWavFilePath = convolverDir + "Stereo" + samplingRate + "CUSTOM" + "_" + customFileNumber + ".wav";
       
        parentWindow.setStatus("Generating custom " + samplingRate + " stereo WAV file");
        try { sleep(1);} catch (InterruptedException ie) {}

        SoxProcessor sp = new SoxProcessor(options);
        sp.createWavFromRawPcm(leftPcmFilePath, rightPcmFilePath, outputWavFilePath, samplingRate, false);
    }
    
    private void generateConvolverConfigFile() {
		parentWindow.setStatus("Generating custom " + samplingRate + " convolver configuration");
        try { sleep(1);} catch (InterruptedException ie) {}

		try {
		      PrintWriter out = new PrintWriter(new FileWriter(options.getRoomCorrectionRootPath() + "/JConvolverConfigs/jconvolverCUSTOM" + samplingRate + "_" + customFileNumber + ".conf", false));
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
		      out.println("/impulse/read   1  1    1     0      0       0       1         Stereo" + samplingRate + "CUSTOM_" + customFileNumber + ".wav");
		      out.println("/impulse/read   2  2    1     0      0       0       2         Stereo" + samplingRate + "CUSTOM_" + customFileNumber +  ".wav");
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

}
