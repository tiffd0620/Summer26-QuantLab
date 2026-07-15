%% Step 1: Select EMG CSV file

[file, path] = uigetfile('*.csv', 'Select EMG CSV File');

if isequal(file,0)
    error('No file selected.');
end

filename = fullfile(path, file);

%% Step 2: Load data

datatable = readtable("QuantRecordingKaylee.csv");

% Extract numeric EMG data
EMGdata = datatable{:,:};
[numSamples, numSignals] = size(EMGdata);

%% Step 3: RMS parameters

fs = 1200;                 % Sampling frequency (Hz)
window_ms = 250;            % RMS window (ms), higher number means more smoothing
windowSamples = round(window_ms/1000 * fs);

%% Step 4: Create output matrix

RMSdata = NaN(numSamples, numSignals);

%% Step 5: Process each EMG column

for k = 1:numSignals

    % Extract one EMG signal
    rEMG = EMGdata(:,k);

    % Remove empty cells (NaNs)
    rEMG = rEMG(~isnan(rEMG));

    % Skip empty columns
    if isempty(rEMG)
        continue
    end

    % Remove DC offset
    rEMG = rEMG - mean(rEMG);

    % Compute moving RMS
    RMS = sqrt(movmean(rEMG.^2, windowSamples));

    % Store RMS values
    RMSdata(1:length(RMS),k) = RMS;

end

%% Step 6: Create MATLAB table

RMStable = array2table(RMSdata,...
    'VariableNames', datatable.Properties.VariableNames);

disp('Finished! RMS values are stored in the variable RMStable.');
openvar('RMStable');