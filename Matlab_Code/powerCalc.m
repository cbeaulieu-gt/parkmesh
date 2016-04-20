myDMM = DMM('myDAQ1','Voltage',-1,1); %% change this line
time = zeros(10000000,1);
current = zeros(10000000,1);
tic
l = 1;
while (1)
    k = 1;
    for k = 1:length(time)
       current(k) = myDMM.read();
       time(k) = toc;
    end
    filename = sprintf('Current_Consumption_2016_04_20_Meas_%02d.mat',l);
    save(filename, 'time', 'current');
    l = l + 1;
end

    