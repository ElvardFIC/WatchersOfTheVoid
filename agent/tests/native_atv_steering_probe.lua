local role=os.getenv('VOTVCOOP_NET_ROLE') or 'none'
local evidence=os.getenv('ATV_PROBE_DIR')
local oldDriver=false
LoopAsync(1000,function()
    local heartbeat=io.open(evidence..'/'..role..'-heartbeat.txt','w');if heartbeat then heartbeat:write(os.date('!%H:%M:%S'));heartbeat:close() end
    ExecuteInGameThread(function()
        local ok,err=pcall(function()
            local control=io.open(evidence..'/driver.txt','r')
            if not control then return end
            local command=control:read('*a');control:close()
            local driver,amount=command:match('(%a+)%s+([%-%d%.]+)')
            amount=tonumber(amount) or 0
            local player=nil
            for _,p in ipairs(FindAllOf('mainPlayer_C') or {}) do
                if p:IsValid() and p.Controller:IsValid() then player=p;break end
            end
            local vehicles=FindAllOf('ATV_C') or {}
            for _,vehicle in ipairs(vehicles) do
                if vehicle:IsValid() and vehicle.handleAxis:IsValid() then
                    local key=vehicle.Key:ToString()
                    local found=io.open(evidence..'/'..role..'-keys.txt','a');if found then found:write(key..'\n');found:close() end
                    if key:lower()=='atv' then
                        if driver==role and player then
                            vehicle.player=player
                            vehicle.isDriven=true
                            vehicle.input_control=false
                            vehicle.input_right=amount>0
                            vehicle.input_left=amount<0
                            vehicle.rotAlpha=amount
                            oldDriver=true
                        elseif oldDriver then
                            vehicle.isDriven=false
                            vehicle.input_right=false
                            vehicle.input_left=false
                            vehicle.rotAlpha=0
                            oldDriver=false
                        end
                        local yaw=vehicle.handleAxis.RelativeRotation.Yaw
                        local file=io.open(evidence..'/'..role..'-angles.tsv','a')
                        if file then file:write(string.format('%s\t%s\t%.2f\t%s\n',os.date('!%H:%M:%S'),command:gsub('\n',''),yaw,tostring(vehicle.isDriven)));file:close() end
                    end
                end
            end
        end)
        if not ok then print('[ATV-STEERING-PROBE] '..tostring(err)) end
    end, EGameThreadMethod.ProcessEvent)
    return false
end)
