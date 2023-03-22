var submit_button = null;
var name_input = null;
var authorid = null;
var ws = null;

const send_name = () => {};

const connectWs = (initialize = false) => {
  ws = new WebSocket(`ws://${window.location.host}`);
  ws.onopen = (event) => {};

  ws.onmessage = (event) => {
    let info = JSON.parse(event.data);
    console.log("onmessage", info);
    if (info.subject === undefined || info.subject === null) {
      return;
    } else if (info.subject === "set_username") {
      set_cookie("authorid", info.authorid, 7);
      console.log(info.authorid);
      window.location.pathname = "/chat.html";
    }
  };

  ws.onclose = (event) => {
    console.log("onclose", event.data);
    setTimeout(() => {
      connectWs(false);
    }, 1000);
    // show error message?
  };
};

window.onload = () => {
  submit_button = document.getElementById("submit");
  name_input = document.getElementById("name");
  authorid = get_cookie("authorid");

  if (submit_button === undefined || name_input === undefined) {
    return;
  }

  submit_button.onclick = (event) => {
    send_user_name();
  };

  name_input.onkeydown = (event) => {
    if (event.key === "Enter") {
      send_user_name();
    }
  };

  const send_user_name = () => {
    console.log("send_user_name");
    if (ws === null || name_input === null) {
      return;
    }
    if (name_input.value === null || name_input.value.trim() === "") {
      return;
    }
    console.log("ws.send")
    ws.send(
      JSON.stringify({
        cmd: "request",
        args: ["set_username", name_input.value],
      })
    );
  };

  if (authorid !== null) {
    window.location.pathname = "/chat.html";
  }
  connectWs();
};
