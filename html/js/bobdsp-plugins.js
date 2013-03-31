function BobDSPPlugins(pluginelements)
{
  var clientsdiv = pluginelements.clientsdiv;

  var clients = new Array();
  var clientindex  = -1;
  var controlindex = -1;

  function loadClients()
  {
    $.getJSON("clients", loadPostSuccess);
  }

  function loadPostSuccess(data)
  {
    parseClients(data);

    var postjson = {action: "wait", timeout: 60000, clientindex: clientindex, controlindex: controlindex};
    $.post("clients", JSON.stringify(postjson), loadPostSuccess);
  }

  var inpost = false;
  var updateinterval = 50;
  var lastupdate = new Date().getTime() - updateinterval;

  function checkClientsCallback()
  {
    var postjson = new Object();
    postjson.clients = new Array();

    for (var i = 0; i < clients.length; i++)
    {
      var clientupdate = new Object();
      clientupdate.controls = new Array();

      var updated = false;
      for (var j = 0; j < clients[i].controls.length; j++)
      {
        if (clients[i].controls[j].changed)
        {
          clients[i].controls[j].changed = false;
          updated = true;
          if (j == 0)
            clientupdate.instances = clients[i].controls[j].value;
          else if (j == 1)
            clientupdate.pregain = clients[i].controls[j].value;
          else if (j == 2)
            clientupdate.postgain = clients[i].controls[j].value;
          else
            clientupdate.controls.push({name: clients[i].controls[j].name,
                                        value: clients[i].controls[j].value});
        }
      }

      if (updated)
      {
        clientupdate.action = "update";
        clientupdate.name = clients[i].client.name;

        if (clientupdate.controls.length == 0)
          delete clientupdate.controls;

        postjson.clients.push(clientupdate);
      }
    }

    //TODO: add a callback for when the POST fails
    if (postjson.clients.length > 0)
    {
      lastupdate = new Date().getTime();
      $.post("clients", JSON.stringify(postjson), controlPostSuccess);
    }
    else
    {
      inpost = false;
    }
  }

  function sendClientsUpdates()
  {
    if (!inpost)
    {
      inpost = true;

      var now = new Date().getTime();
      var sincelast = now - lastupdate;

      if (sincelast >= updateinterval)
        checkClientsCallback();
      else
        setTimeout(checkClientsCallback, updateinterval - sincelast);
    }
  }

  function controlPostSuccess(data)
  {
    inpost = false;
    parseClients(data);
    sendClientsUpdates();
  }

  function parseClients(data)
  {
    if (data.clientindex > clientindex)
    {
      //clients changed, remove all and rebuild
      clientindex = data.clientindex;
      controlindex = data.controlindex;
      makeClients(data);
    }
    else if (data.controlindex > controlindex)
    {
      //update controls of existing clients
      controlindex = data.controlindex;
      updateClients(data);
    }
  }

  function updateClients(data)
  {
    for (var i = 0; i < clients.length; i++)
      updateClient(clients[i], data.clients[i]);
  }

  function updateClient(client, clientdata)
  {
    updateClientControl(client.controls[0], clientdata.instances);
    updateClientControl(client.controls[1], clientdata.pregain);
    updateClientControl(client.controls[2], clientdata.postgain);

    for (var i = 3; i < client.controls.length; i++)
      updateClientControl(client.controls[i], clientdata.controls[i - 3].value);
  }

  function makeClients(data)
  {
    //remove old clients
    for (var i = 0; i < clients.length; i++)
      clientsdiv.get(0).removeChild(clients[i].div);

    clients = new Array();
    for (var i = 0; i < data.clients.length; i++)
      addClient(data.clients[i]);
  }

  function addClient(clientdata)
  {
    var client = new Object();
    client.client = clientdata;

    clients.push(client);

    client.div = document.createElement("div");
    client.div.setAttribute("class", "ui-widget-content ui-corner-all");
    client.div.style.border = "10px solid transparent";
    clientsdiv.get(0).appendChild(client.div);

    var table = document.createElement("table");
    client.div.appendChild(table);
    table.style.width = "500px";

    table.appendChild(makeClientTitle(client.client.name));

    client.controls = new Array();
    client.controls[0] = makeClientSpinner(client, "instances", client.client.instances, 1, 20);
    client.controls[1] = makeClientSlider(client, "pregain", client.client.pregain, 0, 4, true);
    client.controls[2] = makeClientSlider(client, "postgain", client.client.postgain, 0, 4, true);

    for (var i = 0; i < client.client.controls.length; i++)
    {
      var control = client.client.controls[i];

      var min;
      if (control.lowerbound != undefined)
        min = control.lowerbound;
      else
        min = null;

      var max;
      if (control.upperbound != undefined)
        max = control.upperbound;
      else
        max = null;

      var row;
      if (control.integer)
        row = makeClientSpinner(client, control.name, control.value, min, max);
      else if (control.toggled)
        row = makeClientCheckbox(client, control.name, control.value, i);
      else if (min != null && max != null)
        row = makeClientSlider(client, control.name, control.value, min, max, control.logarithmic);
      else
        row = makeClientInput(client, control.name, control.value);

      client.controls.push(row);
    }

    for (var i = 0; i < client.controls.length; i++)
      table.appendChild(client.controls[i].row);
  }

  function makeClientTitle(name)
  {
    var titlerow = document.createElement("tr");
    var title    = document.createElement("th");

    title.colSpan = 0;
    title.style.textAlign = "left";
    $(title).text(name);
    titlerow.appendChild(title);

    return titlerow;
  }

  function makeClientControlRow(name)
  {
    var row = document.createElement("tr");

    var nametd = document.createElement("td");
    $(nametd).text(name);
    row.appendChild(nametd);
    nametd.style.width = "35%";

    var inputtd = document.createElement("td");
    row.appendChild(inputtd);
    inputtd.style.width = "15%";

    var input = document.createElement("input");
    inputtd.appendChild(input);
    input.style.width = "100%";
    input.onfocus = function() {this.select();};

    return { name: name, row: row, input: input, inputtd: inputtd};
  }

  function updateClientControl(controlrow, value)
  {
    if (!controlrow.changed && controlrow.value != value)
    {
      controlrow.value = value;

      if (controlrow.input.getAttribute("type") == "checkbox")
      {
        var checked = Math.round(value) ? true : false;
        controlrow.input.checked = checked;
        $(controlrow.label).text(checked ? "on" : "off");
        $(controlrow.input).button("refresh");
      }
      else
      {
        controlrow.input.value = value;
        if (controlrow.slider != undefined)
        {
          var slidervalue = valueToSlider(controlrow.log, value, controlrow.min, controlrow.max);
          $(controlrow.slider).slider("value", slidervalue);
        }
      }
    }
  }

  function onSlide(controlrow)
  {
    return function(event, ui)
    {
      if (ui.value != controlrow.value)
      {
        controlrow.value = sliderToValue(controlrow.log, ui.value, controlrow.min, controlrow.max);
        controlrow.input.value = controlrow.value;
        controlrow.changed = true;

        sendClientsUpdates();
      }
    }
  }

  function onChange(controlrow)
  {
    return function()
    {
      var value;
      if (controlrow.input.getAttribute("type") == "checkbox")
      {
        value = controlrow.input.checked ? 1 : 0;
        $(controlrow.label).text(controlrow.input.checked ? "on" : "off");
      }
      else
      {
        value = parseFloat(controlrow.input.value);
      }

      if (!isNaN(value) && value != controlrow.value)
      {
        controlrow.value = value;
        controlrow.changed = true;

        if (controlrow.slider != undefined)
        {
          var slidervalue = valueToSlider(controlrow.log, value, controlrow.min, controlrow.max);
          $(controlrow.slider).slider("value", slidervalue);
        }

        sendClientsUpdates();
      }
    }
  }

  var base = 10.0;
  function sliderToValue(log, value, min, max)
  {
    var slidervalue;
    if (log)
    {
      var range = max - min;
      var scaled = ((float = value) - min) / range;
      slidervalue = (Math.pow(base, scaled) - 1) / (base - 1) * range + min;

    }
    else
    {
      slidervalue = value;
    }

    var decimals = 100;
    if (slidervalue != 0)
    {
      while (Math.abs(slidervalue * decimals) < 100)
        decimals *= 10;
    }

    return Math.round(slidervalue * decimals) / decimals;
  }

  function valueToSlider(log, value, min, max)
  {
    if (log)
    {
      var range = max - min;
      var scaled = ((float = value) - min) / range;

      if (scaled <= 0)
        return min;
      else if (scaled >= 1)
        return max
      else
        return Math.log(scaled * (base - 1) + 1) / Math.log(base) * range + min;
    }
    else
    {
      return Math.min(Math.max(value, min), max);
    }
  }

  function makeClientSlider(client, name, value, min, max, log)
  {
    var controlrow = makeClientControlRow(name);

    controlrow.log = log;
    controlrow.min = min;
    controlrow.max = max;

    controlrow.input.value = value;
    controlrow.input.onchange = onChange(controlrow);
    controlrow.input.setAttribute("class", "ui-widget-content ui-corner-all");

    var slidertd = document.createElement("td");
    controlrow.row.appendChild(slidertd);
    slidertd.style.width = "50%";
    slidertd.style.borderLeft  = "10px solid transparent";
    slidertd.style.borderRight = "10px solid transparent";

    controlrow.slider = document.createElement("div");
    slidertd.appendChild(controlrow.slider);

    controlrow.value = value;
    controlrow.changed = false;

    var step = (max - min) / 100;
    if (step <= 0)
      step = 0.01;

    var sliderparms =
    {
      min: min,
      max: max,
      step: step,
      value: valueToSlider(log, value, min, max),
      slide: onSlide(controlrow),
      animate: "fast"
    };

    $(controlrow.slider).slider(sliderparms);

    return controlrow;
  }

  function makeClientSpinner(client, name, value, min, max)
  {
    var controlrow = makeClientControlRow(name);

    controlrow.value = value;
    controlrow.changed = false;

    var spinnerparms =
    {
      incremental: false,
      min: min,
      max: max,
      stop: onChange(controlrow)
    };

    $(controlrow.input).spinner(spinnerparms);
    controlrow.input.value = value;

    return controlrow;
  }

  function makeClientCheckbox(client, name, value, index)
  {
    var controlrow = makeClientControlRow(name);

    controlrow.value = value;
    controlrow.changed = false;

    var checked = Math.round(value) ? true : false;
    controlrow.input.setAttribute("type", "checkbox");
    controlrow.input.checked = checked;

    var checkid = client.client.name + name + index;
    controlrow.input.setAttribute("id", checkid);
    controlrow.input.onchange = onChange(controlrow);

    controlrow.label = document.createElement("label");
    controlrow.label.setAttribute("for", checkid);
    controlrow.label.style.width = "100%";
    controlrow.inputtd.appendChild(controlrow.label);

    $(controlrow.input).button();
    $(controlrow.label).text(checked ? "on" : "off");

    return controlrow;
  }

  function makeClientInput(client, name, value)
  {
    var controlrow = makeClientControlRow(name);

    controlrow.input.value = value;
    controlrow.input.onchange = onChange(controlrow);
    controlrow.input.setAttribute("class", "ui-widget-content ui-corner-all");

    controlrow.value = value;
    controlrow.changed = false;

    return controlrow;
  }


  loadClients();
}