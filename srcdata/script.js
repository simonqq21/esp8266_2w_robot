let gateway = `ws://${window.location.hostname}/ws`;
    let websocket; 
    // status update JSON format
    let robotStatus = {   
        'type': 'status',
        'lightState': false,
        'beepState': false,
        'lmotor_power': 0,
        'rmotor_power': 0,
        'uds_distance': 0.0
    }
    // power and trim increment values
    let powerInc = 2.5; 
    let trimInc = 0.025; 
    let turnSpdPercentage = 0.7; 
    let defaultPower = 160;
    let defaultTrim = 0;
    let powerTrimChangeInterval; 
    let powerTrimChangeTimeout;

    function initWebSocket() {
        // alert('Websocket initializing');
        websocket = new WebSocket(gateway);
        websocket.onopen = onOpen; 
        websocket.onclose = onClose; 
        websocket.onmessage = onMessage;
    }
    // runs when websocket opens
    function onOpen(event) {
        // alert('Connection opened');
        // JSON to request for a status update
        requestStatus();
    }
    function requestStatus() {
        jsondata = {'type': 'status'}
        websocket.send(jsondata);
        updateStatusIndicators();
    }
    // runs when websocket closes
    function onClose(event) {
        // alert('Connection closed');
        setTimeout(initWebSocket, 2000); // restart websocket
    }
    // runs when websocket receives message from server
    function onMessage(event) {
        // console.log(event.data);
        robotStatus = JSON.parse(event.data);
        // update sensor readings 
        updateSensorReadings();
    } 
    // update the status indicators on the webpage
    function updateStatusIndicators() {
        let lightsButton = $("#lights");
        let beepButton = $("#beep");
        // update beep button status
        if (robotStatus.beepState) {
            beepButton.addClass("onbutton");
            beepButton.removeClass("offbutton");
        }
        else {
            beepButton.addClass("offbutton");
            beepButton.removeClass("onbutton");
        }
        // update lights button status
        if (robotStatus.lightState) {
            lightsButton.addClass("onbutton");
            lightsButton.removeClass("offbutton");
        }
        else {
            lightsButton.addClass("offbutton");
            lightsButton.removeClass("onbutton");
        }
    }
    
    // update sensor value fields 
    function updateSensorReadings() {
        let UDS_reading = parseFloat(robotStatus.uds_distance);
        $("#UDS1_display").text(UDS_reading);
    }


    $(document).ready(function() {
        getPowerValue(); 
        getTrimValue();
        updateStatusIndicators();
        if (isTouchEnabled()) {
            for (element of $("input, button")) {
                element.onmousedown = ""; 
                element.onmouseup = "";
            }    
        }
        else {
            for (element of $("input, button")) {
                element.ontouchstart = ""; 
                element.ontouchend = "";
            } 
        }
        initWebSocket();
        setInterval(requestStatus, 200); 
    });
    /*
    power = power slider value, from 0 to 255, default 100
    trim = trim slider value, from -1 to 1, default 0
    The power is the maximum power value of either or both motors. 
    The trim is the difference in power between the left and the right motors to adjust for any deviations in direction when the 
    robot is moving straight.

    The two wheeled robot has two motors, the left motor and the right motor. 
    The two motors may have a different speed even with the same power values. 

    when forward is pressed, both motors move forward at ideally the same speed.
    lmotor_speed = power * (1 + trim)
    rmotor_speed = power * (1 - trim)

    when backward is pressed, both motors move backward at ideally the same speed. 
    lmotor_speed = -power * (1 + trim)
    rmotor_speed = -power * (1 - trim)

    when left is pressed, the right motor moves forward while the left motor moves backward at ideally the same speed. 
    lmotor_speed = -turnSpdPercentage * power * (1 + trim)
    rmotor_speed = turnSpdPercentage * power * (1 - trim)

    when right is pressed, the left motor moves forward while the right motor moves backward at ideally the same speed. 
    lmotor_speed = turnSpdPercentage * power * (1 + trim)
    rmotor_speed = -turnSpdPercentage * power * (1 - trim)
    
    // angled left and angled right to be continued
    */ 

    // control the motors given the power and trim adjustment values.
    // power is power value from 0 to 255
    // trim is trim value from -1 to 1
    // direction is one of five, "forward", "backward", "left", "right", "stop"
    function move(event, direction) {
        event.preventDefault();
        let power = getPowerValue();
        let trim = getTrimValue();
        let motor_settings = {
            'type': 'motors',
            'lmotor_power': 0,
            'rmotor_power': 0
        }
        switch (direction) {
            case "forward":
                // console.log("trim = ", trim);
                motor_settings.lmotor_power = power * (1 + trim); 
                motor_settings.rmotor_power = power * (1 - trim);
                $('#forward').addClass('pressedbutton'); 
                // console.log('forward');
                break;
            case "backward": 
                motor_settings.lmotor_power = -power * (1 + trim); 
                motor_settings.rmotor_power = -power * (1 - trim);
                $('#backward').addClass('pressedbutton'); 
                // console.log('backward');
                break; 
            case "left": 
                motor_settings.lmotor_power = -turnSpdPercentage * power * (1 + trim); 
                motor_settings.rmotor_power = turnSpdPercentage * power * (1 - trim);
                $('#left').addClass('pressedbutton'); 
                // console.log('left');
                break; 
            case "right": 
                motor_settings.lmotor_power = turnSpdPercentage * power * (1 + trim); 
                motor_settings.rmotor_power = -turnSpdPercentage * power * (1 - trim);
                $('#right').addClass('pressedbutton'); 
                // console.log('right');
                break; 
            case "brake": 
                motor_settings.lmotor_power = -999; 
                motor_settings.rmotor_power = -999;
                $('#brake').addClass('pressedbutton'); 
                // console.log('brake');
                break; 
            case "stop":
            default:
                $('.dpad_button').removeClass('pressedbutton'); 
                // console.log('stop'); 
                break;
        }
        // console.log("motor json = ", JSON.stringify(motor_settings));
        websocket.send(JSON.stringify(motor_settings));
    }
    function setPower(power) {
        $("#power").val(power); 
        $("#power_display").text($("#power").val()); 
        // console.log('power=', power);
    }
    function setTrim(trim) {
        $("#trim").val(trim); 
        $("#trim_display").text($("#trim").val());  
        // console.log("trim=", trim);
    }
    function changePower(deltaPower) {
        power = getPowerValue(); 
        power += deltaPower; 
        power = Math.min(power, 255); 
        power = Math.max(power, 0);
        setPower(power);
    }
    function changeTrim(deltaTrim) { 
        trim = getTrimValue();
        trim += deltaTrim; 
        trim = Math.min(trim, 1); 
        trim = Math.max(trim, -1);
        setTrim(trim);
    }
    function getPowerValue() {
        let power = $("#power").val();  
        power = parseInt(power);
        let powerDisplay = $("#power_display");
        $("#power_display").text(power); 
        return power;
    }
    function getTrimValue() {
        let trim = $("#trim").val(); 
        trim = parseFloat(trim);
        let trimDisplay = $("#trim_display");
        $("#trim_display").text(trim);
        return trim;
    }
    function holdPowerTrimButtons(ev, mode, value) {
        ev.preventDefault();
        switch (mode) {
            case "power":
                powerTrimChangeTimeout = setTimeout(function() {
                    powerTrimChangeInterval = setInterval(changePower, 50, value);
                }, 500);
                break;
            case "trim":
                powerTrimChangeTimeout = setTimeout(function() {
                    powerTrimChangeInterval = setInterval(changeTrim, 50, value);
                }, 500);
                break;
            default:
        }
    }

    function releasePowerTrimButtons(ev) {
        ev.preventDefault();
        clearTimeout(powerTrimChangeTimeout); 
        clearInterval(powerTrimChangeInterval);
    }
    // function startInterval(interval, handler, timeoutDelay, intervalDelay, value) {
    //     return 
    // }
    // function stopInterval(timeout, interval) {
    //     clearTimeout(timeout); 
    //     clearInterval(interval);
    // }
    function keyupHandler(event) {
        let key = event.key;  
        switch(key) {
            // off horn
            case 'h': 
            case 'H': 
                toggleBeep(event, 'off');
                break;
            // stop when movement keys is released 
            case 'w':
            case 'W':
            case 'a':
            case 'A':
            case 's':
            case 'S':
            case 'd':
            case 'D':
            case ' ':
                move(event, '');
        }
    }

    function keydownHandler(event) {
        let key = event.key; 
        switch (key) {
            // forward
            case 'w':
            case 'W':
                move(event, 'forward');
                break;
            // left 
            case 'a':
            case 'A':
                move(event, 'left');
                break;
            // reverse 
            case 's':
            case 'S':
                move(event, 'backward');
                break;
            // right
            case 'd':
            case 'D':
                move(event, 'right');
                break;
            // decrease power 
            case '-': 
                changePower(-powerInc);
                break;
            // increase power 
            case '=': 
                changePower(powerInc);
                break;
            // decrease trim 
            case '[': 
                changeTrim(-trimInc);
                break;
            // increase trim 
            case ']': 
                changeTrim(trimInc);
                break;
            // toggle lights 
            case 'l': 
            case 'L': 
                toggleLights(event, 'toggle');
                break;
            // sound horn 
            case 'h': 
            case 'H': 
                toggleBeep(event, 'on');
                break; 
            // ebrake 
            case ' ': 
                move(event, 'brake');
                break;
            // reset power 
            case 'r':
            case 'R':
                setPower(defaultPower);
                break;
            // reset trim 
            case 't':
            case 'T':
                setTrim(defaultTrim);
                break;
            default:
                break;
        }
    }
    // toggle the lights on the car 
    // state is the string state of the lights, either 'on', 'off', or 'toggle'
    function toggleLights(event, state) {
        event.preventDefault();
        let stateVal = false;
        switch (state) {
            case 'on':
                stateVal = true;
                break;
            
            case 'toggle': 
                stateVal = !robotStatus.lightState;
                break;

            default:
                break;
        }
        let light_settings = {
            'type': 'lights', 
            'lights_state': stateVal
        }
        websocket.send(JSON.stringify(light_settings));
        updateStatusIndicators();
    }
    // toggle the horn on the car 
    // state is the string state of the horn, either 'on' or 'off'
    function toggleBeep(event, state) {
        event.preventDefault();
        let stateVal = false;
        switch (state) {
            case 'on':
                stateVal = true;
                break;
        
            default:
                break;
        }
        let horn_settings = {
            'type': 'beep', 
            'horn_state': stateVal
        }
        websocket.send(JSON.stringify(horn_settings));
        updateStatusIndicators();
    }

    function isTouchEnabled() {
        return ('ontouchstart' in window) || 
            (navigator.maxTouchPoints > 0) || 
            (navigator.msMaxTouchPoints > 0);
    }

    function isTouchInsideElement(event, element) {
        const rect = element.getBoundingClientRect();
        return (
            event.touches[0].clientX >= rect.left &&
            event.touches[0].clientX <= rect.right &&
            event.touches[0].clientY >= rect.top &&
            event.touches[0].clientY <= rect.bottom
        );
    }

    function stopOnTouchLeave(event) {
        if (!isTouchInsideElement(event, event.target)) {
            move(event, '');
        }
    }

    function stopHornOnTouchLeave(event) {
        if (!isTouchInsideElement(event, event.target)) {
            toggleBeep(event, 'off')
        }
    }