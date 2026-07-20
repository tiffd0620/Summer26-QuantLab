% load data 
datatable = readtable('KayleeRMS.csv');

% extract only numeric columns (ignoring headers) 
RMSdata = datatable{:, :};   % assumes all columns are ECG
[numSamples, numSignals] = size(RMSdata);

fs = 1200; 
t = (0:numSamples-1)'/fs;

% empty array to store peak values 
all_peak_y_values = [];

% loop through RMS columns 
Volts_all = cell(numSignals,1);   % store Hz for each signal
Rpeaks_all = cell(numSignals,1);

for k = 4:4 % change to ":numSignals" for every column 
    x = RMSdata(:,k);

    % high pass filtering 
    windowSize = fs;  % 1-second movi0085ng average
    baseline = movmean(x, windowSize);
    filtered = x - baseline; 

    %plotting original graph without peak identification 
    figure;
    plot(t, x); 
    title(['ECG Signal ' num2str(k) ' with R-peaks']);
    xlabel('Time (s)'); 

    % finding Rpeaks 
    numSecs=input('number of seconds between peaks: '); % number of seconds between each contraction 
    minValue=input('smallest peak value: '); 
    maxValue=input('largest peak value: '); 
    windowNum=input('total run time: '); 
    numFiltered=(minValue/maxValue)*100; 
    minPeakDist = numSecs * fs; 
    threshold = prctile(filtered, numFiltered);

    [pks, locs] = findpeaks(x, ...
        'MinPeakDistance', minPeakDist, ...
        'MinPeakHeight', threshold);

    % printing peaks into graph 
    for i = 1:length(pks)
        fprintf('Trial %d: Peak RMS Y-value = %.4f\n', k, pks(i));
    end

    % saving valyes into the table 
    all_peak_y_values = [all_peak_y_values; pks(:)]; 
    
    Rpeaks_all{k} = t(locs);

    Rpeaks_all{k} = t(locs);

    %% Step 5: Compute IBI (R-R intervals)
    RMS = diff(t(locs));
    Volts_all{k} = RMS;

    %% OPTIONAL: Plot for verification
    figure;
    plot(t, x); hold on;
    plot(t(locs), x(locs), 'ro');
    title(['RMS Signal ' num2str(k) ' with R-peaks']);
    xlabel('Time (s)');

    windowTotal = round(windowNum * fs); 
    
    % makes sure the window size doesn't accidentally exceed the signal length
    if windowTotal > length(x)
        windowTotal = length(x);
    end
    
    % calculate a single mean baseline value
    signal_window = x(1:windowTotal);
    baseline_cutoff = prctile(signal_window, 15); 
    fprintf('baseline cutoff value = %.4f\n', baseline_cutoff); 

end