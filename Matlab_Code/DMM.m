classdef DMM < handle
    %DMM Interface to National Instruments DMM channels
    % DMM provides access to the National Instruments DMM channels
    % available on devices like the myDAQ and Elvis II
    %
    % obj = DMM(deviceID,measurmentType,min,max) creates an object OBJ
    % representing the device with the DEVICEID assigned by the National
    % Instruments Measurement and Automation Explorer, with DMM channel
    % 'deviceID/dmm'
    %
    % Call the READ method on the object to retrieve voltage or current DMM
    % readings in Volts or Amps.
    %
    % This requires the Data Acquisition Toolbox and MATLAB R2010b or
    % later.  This object does not take exclusive access to the hardware,
    % and hence can be used in conjunction with a DAQ Session.
    %
    % Example:
    % myDMM = DMM('dev1','Voltage',-10,10);
    % 
    % myDMM.read()
    % ans =
    %    4.871
    %
    % % release the hardware
    % clear myDMM
    
    % Copyright 2013, The MathWorks, Inc.
    
    properties(SetAccess = private)
        Min
        Max
        MeasurementType
    end
    
    methods
        function [obj] = DMM(deviceID, measurementType, min, max)
            if nargin ~= 4
                error('daq:dmm:badParameter','You must provide a valid device identifier, such as ''Dev1'', and a measurement type such as ''Voltage'' or ''Current'', and min and max values such as ''-10'', ''10'' for a voltage measurement or ''-2'', ''2'' for a current measurement.');
            end
            if ~ischar(deviceID)
                error('daq:dmm:badDeviceID','You must provide a valid device identifier, such as ''Dev1''.');
            end
            if ~ischar(measurementType)
                error('daq:dmm:badMeasurementType','You must provide a valid measurement type, such as ''Voltage'' or ''Current''.');
            end
            if strcmpi(measurementType, 'Voltage')
                obj.MeasurementType = 'Voltage';
            elseif strcmpi(measurementType, 'Current')
                obj.MeasurementType = 'Current';
            else
                error('daq:dmm:badMeasurementType','You must provide a valid measurement type, such as ''Voltage'' or ''Current''.');
            end
            if ~isscalar(min) || ~isnumeric(min) || ~isscalar(max) || ~isnumeric(max)
                error('daq:dmm:badMinMax','You must provide valid scalar numeric min and max values such as ''-10'', ''10'' for a voltage measurement or ''-2'', ''2'' for a current measurement.');
            end
            
            obj.Min = min;
            obj.Max = max;
            
            try
                [status,taskHandle] = daq.ni.NIDAQmx.DAQmxCreateTask (char(0),uint64(0));
                obj.throwOrWarnOnStatus(status);
                obj.Task = taskHandle;

                if strcmp(obj.MeasurementType, 'Voltage')
                    [status] = daq.ni.NIDAQmx.DAQmxCreateAIVoltageChan (...
                        taskHandle,...                              % The task handle
                        [deviceID '/dmm'],...                       % physicalChannel
                        char(0),...                                 % nameToAssignToChannel
                        daq.ni.NIDAQmx.DAQmx_Val_Cfg_Default,...    % terminalConfig
                        double(min),...                             % minVal
                        double(max),...                             % maxVal
                        daq.ni.NIDAQmx.DAQmx_Val_Volts,...          % units
                        char(0));                                   % customScaleName
                    obj.throwOrWarnOnStatus(status);
                else
                    [status] = daq.ni.NIDAQmx.DAQmxCreateAICurrentChan (...
                        taskHandle,...                              % The task handle
                        [deviceID '/dmm'],...                       % physicalChannel
                        char(0),...                                 % nameToAssignToChannel
                        daq.ni.NIDAQmx.DAQmx_Val_Cfg_Default,...    % terminalConfig
                        double(min),...                             % minVal
                        double(max),...                             % maxVal
                        daq.ni.NIDAQmx.DAQmx_Val_Amps,...           % units
                        daq.ni.NIDAQmx.DAQmx_Val_Default,...        % shuntResistorLoc
                        1e-6,...                                    % extShuntResistorVal
                        char(0));                                   % customScaleName
                obj.throwOrWarnOnStatus(status);
                end
            
            catch e
                if strcmp(e.identifier,'MATLAB:undefinedVarOrClass')
                    % Check on DAQ not present
                    if ~license('test','data_acq_toolbox')
                        error('daq:dmm:requiresDAQ','This requires the Data Acquisition Toolbox.  For more information, visit the <a href="http://www.mathworks.com/products/daq">Data Acquisition Toolbox web page</a>.');
                    end
                    
                    % Check release -- requires R2010b or later
                    releaseVersion = version('-release');
                    year = str2double(releaseVersion(1:end-1));
                    releaseInYear = releaseVersion(end);
                    if year < 2010 ||...
                            (year == 2010 && strcmpi(releaseInYear,'a'))
                        error('daq:dmm:requiresR2010bOrLater','This requires release R2010b or later.  For more information, visit the <a href="http://www.mathworks.com/products/matlab">MATLAB web page</a>.');
                    end
                elseif strcmp(e.identifier, 'daq:ni:err200170')
                    error('daq:dmm:nodmmchannel','This device does not have ''dmm'' channels.');
                end
                
                [~] = daq.ni.NIDAQmx.DAQmxClearTask(obj.Task);
                obj.Task = [];
                rethrow(e)
            end
        end
        
        function [data] = read(obj)
                [status,data,~,~] =...
                    daq.ni.NIDAQmx.DAQmxReadAnalogF64(...
                    obj.Task,...                                                    % taskHandle
                    int32(1),...                                                    % numSampsPerChan
                    1.0,...                                                         % timeout
                    uint32(daq.ni.NIDAQmx.DAQmx_Val_GroupByScanNumber),...          % fillMode
                    zeros(1,1),...                                                  % readArray 
                    uint32(1),...                                                   % arraySizeInSamps
                    int32(0),...                                                    % sampsPerChanRead
                    uint32(0));                                                     % reserved
            obj.throwOrWarnOnStatus(status);
        end
        
        function delete(obj)
            if ~isempty(obj.Task)
                [~] = daq.ni.NIDAQmx.DAQmxClearTask(obj.Task);
                obj.Task = [];
            end
        end
    end
    
    properties(GetAccess = private,SetAccess = private)
        Task = [];
    end
    
    methods(Static, Access = private)
        function throwOrWarnOnStatus( niStatusCode )
            if niStatusCode == daq.ni.NIDAQmx.DAQmxSuccess
                return
            end
            
            % Capture the extended error string
            % First, find out how big it is
            [numberOfBytes,~] = daq.ni.NIDAQmx.DAQmxGetExtendedErrorInfo(' ', uint32(0));
            % Now, get the message
            [~,extMessage] = daq.ni.NIDAQmx.DAQmxGetExtendedErrorInfo(blanks(numberOfBytes), uint32(numberOfBytes));
            
            if niStatusCode < daq.ni.NIDAQmx.DAQmxSuccess
                % Status code is less than 0 -- It is a NI-DAQmx error, throw an error
                errorToThrow = MException(sprintf('daq:ni:err%06d',-1 * niStatusCode),...
                    'NI Error %06d:\n%s', niStatusCode,extMessage);
                throwAsCaller(errorToThrow)
            else
                % It is a NI-DAQmx error, warn
                warning(sprintf('daq:ni:warn%06d',niStatusCode),...
                    'NI Warning %06d:\n%s',niStatusCode,extMessage);
            end
        end
    end
end
