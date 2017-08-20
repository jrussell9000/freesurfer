
if (exist('subject_index') == 0)
   subject_index = 1 ;
end
iob_table = activeInsulin_3Hr ;
time_series = extract_time(medtronic_subject_data,subject_index) ;
carb_vec = extract_carbs(medtronic_subject_data,subject_index,time_series) ;
bolus_vec = extract_insulin_bolus(medtronic_subject_data,subject_index,time_series) ;
basal_vec = extract_basals(medtronic_subject_data,subject_index,time_series) ;
active_insulin = compute_active_insulin(medtronic_subject_data,subject_index,time_series,bolus_vec,basal_vec,iob_table3) ;
bg_vec = cell2mat(medtronic_subject_data(subject_index).rows(:,bg_reading_ind)) ;
ntps = length(time_series) ;
sensor_vec = cell2mat(medtronic_subject_data(subject_index).rows(:,sensor_glucose_ind)) ;
for iter=1:10
for t=2:ntps-1
    if (isnan(sensor_vec(t)))   % it is nan because another event occurred at same time
       if (time_series(t+1) == time_series(t))
         sensor_vec(t) = sensor_vec(t+1) ;	
       else
          sensor_vec(t) = sensor_vec(t-1) ;	
       end
    end
end
end
for t=2:ntps-1
   if (isnan(sensor_vec(t)))   % it is nan because another evetn occurred at same time
       if (isnan(sensor_vec(t+1)))
         sensor_vec(t) = sensor_vec(t-1) ;	
       else
          sensor_vec(t) = sensor_vec(t+1) ;	
       end
    end
end
% compute active insulin without basal
active_insulin = compute_active_insulin(medtronic_subject_data,subject_index,time_series,bolus_vec,zeros(size(basal_vec)),iob_table3) ;


sensitivity_vec = cell2mat(medtronic_subject_data(subject_index).rows(:,sensitivity_ind)) ;
carb_ratio_vec = cell2mat(medtronic_subject_data(subject_index).rows(:,carb_ratio_ind)) ;
