function BobDSPConnections(connectionelements)
{
  var connectionslist    = connectionelements.connectionslist;
  var resetbutton        = connectionelements.resetbutton;
  var applybutton        = connectionelements.applybutton;
  var applyandsavebutton = connectionelements.applyandsavebutton;
  var restorebutton      = connectionelements.restorebutton;
  var addnewbutton       = connectionelements.addnewbutton;
  var outports           = connectionelements.outports;
  var inports            = connectionelements.inports;
  var addportbutton      = connectionelements.addportbutton;
  var addportregexbutton = connectionelements.addportregexbutton;

  function loadConnections()
  {
    $.getJSON("connections", parseConnections);
  };

  function parseConnections(data)
  {
    /*clear the list of connections, then for each connection create an entry in the unordered list
      each entry has an image with two arrows for dragging the connection,
      a button for setting input disconnect, a button for setting output disconnect,
      a button for deleting the connection,
      a text input for setting the input regex, and a text input for setting the output regex*/

    connectionslist.empty();

    for (var i = 0; i < data.connections.length; i++)
      addItem(data.connections[i], i);
  }

  function addItem(connection, index)
  {
    var li = document.createElement("li");
    li.setAttribute("class", "ui-widget-content");

    var tr = document.createElement("tr");
    li.appendChild(tr);

    var span = document.createElement("span");
    span.setAttribute("class", "ui-icon ui-icon-arrowthick-2-n-s");

    var spantd = document.createElement("td");
    spantd.appendChild(span);
    tr.appendChild(spantd);

    function deleteItem(item) { return function() {connectionslist.get(0).removeChild(item);}; };
    var deletebutton = document.createElement("button");
    $(deletebutton).button({icons:{primary:"ui-icon-close"}, text:false});
    $(deletebutton).click(deleteItem(li));

    var deletebuttontd = document.createElement("td");
    deletebuttontd.appendChild(deletebutton);
    tr.appendChild(deletebuttontd);

    var tds = new Array();
    for (var j = 0; j < 4; j++)
    {
      tds[j] = document.createElement("td");

      //make the text inputs expand to the full width
      if (j % 2 == 1)
        tds[j].style.width = "50%";

      tr.appendChild(tds[j]);
    }

    var checkboxes  = new Array();
    var checklabels = new Array();
    var textinputs  = new Array();
    for (var j = 0; j < 2; j++)
    {
      var checkid = (j == 0 ? "outcheck" : "incheck") + index;
      var textid  = (j == 0 ? "outtext"  : "intext") + index;

      checkboxes[j] = document.createElement("input");
      checkboxes[j].setAttribute("type", "checkbox");
      checkboxes[j].setAttribute("id", checkid);
      checkboxes[j].checked = j == 0 ? connection.outdisconnect : connection.indisconnect; 
      
      checklabels[j] = document.createElement("label");
      checklabels[j].setAttribute("for", checkid);

      tds[j * 2].appendChild(checkboxes[j]);
      tds[j * 2].appendChild(checklabels[j]);
      $(checkboxes[j]).button({icons:{primary:"ui-icon-scissors"}, text:false});

      textinputs[j] = document.createElement("input");
      textinputs[j].style.width = "99%";
      textinputs[j].setAttribute("type", "text");
      textinputs[j].setAttribute("id", textid);
      textinputs[j].value = j == 0 ? connection.out : connection.in;
      tds[j * 2 + 1].appendChild(textinputs[j]);
      textinputs[j].setAttribute("class", "ui-widget-content ui-corner-all");
    }

    connectionslist.get(0).appendChild(li);
  }

  function addConnection(outport, inport)
  {
    item = 
    {
      out : outport,
      in : inport,
      indisconnect : false,
      outdisconnect : false
    };

    addItem(item, connectionslist.get(0).length);
  }

  function sendConnections(action)
  {
    /*gets the values from the input and output regex textboxes,
      the values from the input and output textboxes
      and builds a JSON object to send to bobdsp*/

    var postjson = new Object();
    postjson.connections = new Array();

    //these should all be the same length
    var outcheckboxes = connectionslist.find("[id^='outcheck']");
    var outtexts      = connectionslist.find("[id^='outtext']");
    var incheckboxes  = connectionslist.find("[id^='incheck']");
    var intexts       = connectionslist.find("[id^='intext']");

    for (var i = 0; i < outcheckboxes.length; i++)
    {
      postjson.connections[i] =
      {
        out           : outtexts[i].value,
        in            : intexts[i].value,
        outdisconnect : outcheckboxes[i].checked,
        indisconnect  : incheckboxes[i].checked
      };
    }

    if (action != undefined)
      postjson.action = action;

    $.post("/connections", JSON.stringify(postjson), parseConnections);
  }

  function sendAction(action)
  {
    $.post("/connections", JSON.stringify({action : action}), parseConnections);
  }

  function loadPorts()
  {
    $.ajax({ 
      url: "ports", 
      dataType: "json", 
      success: parsePorts, 
      timeout: 10000,
      error: portsErrorHandler
    });
  }

  var portindex = -1;

  function portsErrorHandler()
  {
    //clear the ports list
    outports.empty();
    inports.empty();

    //reset index
    portindex = -1;

    //try to get the ports again
    setTimeout(loadPorts, 1000);
  }

  function parsePorts(data)
  {
    //bobdsp will only send the ports if the index changed
    //on a timeout, it will only send the index
    if (portindex != data.index)
    {
      var selectedoutport = outports.children(".ui-selected");
      var selectedinport  = inports.children(".ui-selected");

      //clear the ports list
      outports.empty();
      inports.empty();

      //add the ports from the JSON data, make sure any selected port stays selected
      for (var i = 0; i < data.ports.length; i++)
      {
        var li = document.createElement("li");
        li.setAttribute("class", "ui-widget-content");
        $(li).text(data.ports[i].name);
        if (data.ports[i].direction == "output")
        {
          outports.get(0).appendChild(li);
          if (selectedoutport.length > 0 && data.ports[i].name == selectedoutport[0].textContent)
            $(li).addClass("ui-selected");
        }
        else if (data.ports[i].direction == "input")
        {
          inports.get(0).appendChild(li);
          if (selectedinport.length > 0 && data.ports[i].name == selectedinport[0].textContent)
            $(li).addClass("ui-selected");
        }
      }

      portindex = data.index;
    }

    //get json again, but set the timeout to 60 seconds and pass the index
    //this way, bobdsp will wait for the ports to change or the timeout to occur
    //before sending the ports
    var timeout = 60000;
    var postjson = JSON.stringify({"timeout" : timeout, "index" : data.index});

    $.ajax({ 
      type: "POST",
      url: "ports", 
      data: postjson,
      dataType: "json", 
      success: parsePorts, 
      timeout: timeout + 10000,
      error: portsErrorHandler
    });
  }

  function addConnectionFromPort(asregex)
  {
    var outport = outports.children(".ui-selected");
    var inport  = inports.children(".ui-selected");
    if (outport.length > 0 && inport.length > 0)
    {
      if (asregex)
        addConnection(regexify(escapeRegex(outport[0].textContent)), regexify(escapeRegex(inport[0].textContent)));
      else
        addConnection(escapeRegex(outport[0].textContent), escapeRegex(inport[0].textContent));
    }
  }

  function escapeRegex(instr)
  {
    var escapechars = ".^$*+?()[]{\\";
    var out = "";

    for (var i = 0; i < instr.length; i++)
    {
      var currchar = instr.charAt(i);
      var found = false;
      //TODO: figure out why escapechars.search(currchar) doesn't work
      for (var j = 0; j < escapechars.length; j++)
      {
        if (currchar == escapechars.charAt(j))
        {
          out += "\\";
          out += currchar;
          found = true;
          break;
        }
      }

      if (!found)
        out += currchar;
    }

    return out;
  }

  //turns any number of the client name into [0-9]*
  function regexify(instr)
  {
    var out = "";
    var innumber = false;
    var inportname = false;
    for (var i = 0; i < instr.length; i++)
    {
      var currchar = instr.charAt(i);
      if (currchar == ':')
        inportname = true;

      if (innumber)
      {
        if (currchar < '0' || currchar > '9')
        {
          innumber = false;
          out += "[0-9]*";
          out += currchar;
        }
      }
      else
      {
        if (!inportname && currchar >= '0' && currchar <= '9')
        {
          if (i == instr.length - 1)
            out += "[0-9]*";
          else
            innumber = true;
        }
        else
        {
          out += currchar;
        }
      }
    }
    return out;
  }

  resetbutton.click(loadConnections);
  applybutton.click(function() {sendConnections();});
  applyandsavebutton.click(function() {sendConnections("save");});
  restorebutton.click(function() {sendAction("reload")});
  addnewbutton.click(function() {addConnection("", "");});
  addportbutton.click(function() {addConnectionFromPort(false);});
  addportregexbutton.click(function() {addConnectionFromPort(true);});

  loadConnections();
  loadPorts();
}
