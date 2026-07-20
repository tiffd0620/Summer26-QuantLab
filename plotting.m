for k = 1:15 % change to ":numSignals" for every column 
    x = RMSdata(:,k);

    %plotting original graph without peak identification 
    figure;
    plot(t, x); 
    title(['ECG Signal ' num2str(k) ' with R-peaks']);
    xlabel('Time (s)'); 

end 